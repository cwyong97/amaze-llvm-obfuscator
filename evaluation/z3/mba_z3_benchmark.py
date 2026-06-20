"""
Mixed Boolean-Arithmetic (MBA) SMT Benchmark Tool
Dynamically generates MBA obfuscation expressions and benchmarks Z3 solver performance 
at varying bit-widths to measure constraint-solving hardness.
"""

import random
import sys
import time
from z3 import *

# Color formatting
CYAN = "\033[1;36m"
GREEN = "\033[1;32m"
YELLOW = "\033[1;33m"
PURPLE = "\033[1;35m"
BOLD = "\033[1m"
RESET = "\033[0m"

MASK = 0xffffffffffffffff

def to_u64(val): return val & MASK

def to_i64(val):
    val = val & MASK
    if val >= 0x8000000000000000:
        return val - 0x10000000000000000
    return val

def modInverse_odd(c):
    c = to_u64(c)
    inv = c
    for _ in range(5):
        inv = to_u64(inv * to_u64(2 - to_u64(c * inv)))
    return inv

def ctz(val):
    val = to_u64(val)
    if val == 0: return 64
    return (val & -val).bit_length() - 1

basis_pool = [
    {"name": "0", "op": lambda x, y: 0},
    {"name": "(x & y)", "op": lambda x, y: to_u64(x & y)},
    {"name": "(x & ~y)", "op": lambda x, y: to_u64(x & ~y)},
    {"name": "x", "op": lambda x, y: to_u64(x)},
    {"name": "(~x & y)", "op": lambda x, y: to_u64(~x & y)},
    {"name": "y", "op": lambda x, y: to_u64(y)},
    {"name": "(x ^ y)", "op": lambda x, y: to_u64(x ^ y)},
    {"name": "(x | y)", "op": lambda x, y: to_u64(x | y)},
    {"name": "(~(x | y))", "op": lambda x, y: to_u64(~(x | y))},
    {"name": "(~(x ^ y))", "op": lambda x, y: to_u64(~(x ^ y))},
    {"name": "(~y)", "op": lambda x, y: to_u64(~y)},
    {"name": "(x | ~y)", "op": lambda x, y: to_u64(x | ~y)},
    {"name": "(~x)", "op": lambda x, y: to_u64(~x)},
    {"name": "(~x | y)", "op": lambda x, y: to_u64(~x | y)},
    {"name": "(~(x & y))", "op": lambda x, y: to_u64(~(x & y))},
    {"name": "-1", "op": lambda x, y: MASK},
]

def get_z3_basis(x, y, name, bit_width):
    if name == "0": return BitVecVal(0, bit_width)
    elif name == "-1": return BitVecVal(((1 << bit_width) - 1), bit_width)
    elif name == "x": return x
    elif name == "y": return y
    elif name == "(~x)": return ~x
    elif name == "(~y)": return ~y
    elif name == "(x & y)": return x & y
    elif name == "(x | y)": return x | y
    elif name == "(x ^ y)": return x ^ y
    elif name == "(x & ~y)": return x & ~y
    elif name == "(~x & y)": return ~x & y
    elif name == "(x | ~y)": return x | ~y
    elif name == "(~x | y)": return ~x | y
    elif name == "(~(x & y))": return ~(x & y)
    elif name == "(~(x | y))": return ~(x | y)
    elif name == "(~(x ^ y))": return ~(x ^ y)
    else: return BitVecVal(0, bit_width)

def compute_basis_matrix():
    A = [[0] * len(basis_pool) for _ in range(4)]
    for i in range(4):
        vx = (i >> 1) & 1
        vy = i & 1
        for j in range(len(basis_pool)):
            A[i][j] = to_i64(basis_pool[j]["op"](vx, vy))
    return A

def solve(A, b, intensity=50):
    rows, cols = len(A), len(A[0])
    D = [[A[r][c] for c in range(cols)] for r in range(rows)]
    S = [[1 if r == c else 0 for c in range(rows)] for r in range(rows)]
    T = [[1 if r == c else 0 for c in range(cols)] for r in range(cols)]

    def sub_row(r1_idx, r2_idx, q):
        for c in range(cols):
            D[r1_idx][c] = to_i64(to_u64(D[r1_idx][c]) - to_u64(q) * to_u64(D[r2_idx][c]))
        for c in range(rows):
            S[r1_idx][c] = to_i64(to_u64(S[r1_idx][c]) - to_u64(q) * to_u64(S[r2_idx][c]))

    def sub_col(c1_idx, c2_idx, q):
        for r in range(rows):
            D[r][c1_idx] = to_i64(to_u64(D[r][c1_idx]) - to_u64(q) * to_u64(D[r][c2_idx]))
        for c in range(cols):
            T[c][c1_idx] = to_i64(to_u64(T[c][c1_idx]) - to_u64(q) * to_u64(T[c][c2_idx]))

    for i in range(min(rows, cols)):
        if D[i][i] == 0:
            found = False
            for r in range(i, rows):
                if found: break
                for c in range(i, cols):
                    if D[r][c] != 0:
                        D[i], D[r] = D[r], D[i]
                        S[i], S[r] = S[r], S[i]
                        for k in range(rows): D[k][i], D[k][c] = D[k][c], D[k][i]
                        for k in range(cols): T[k][i], T[k][c] = T[k][c], T[k][i]
                        found = True
                        break
            if not found: continue

        changed = True
        loop_count = 0
        while changed:
            loop_count += 1
            if loop_count > 100: return None
            changed = False
            for j in range(i + 1, rows):
                while D[j][i] != 0:
                    if D[i][i] == 0:
                        D[i], D[j] = D[j], D[i]
                        S[i], S[j] = S[j], S[i]
                    else:
                        q = int(D[j][i] / D[i][i])
                        sub_row(j, i, q)
                        if D[j][i] != 0:
                            D[i], D[j] = D[j], D[i]
                            S[i], S[j] = S[j], S[i]
                    changed = True
            for j in range(i + 1, cols):
                while D[i][j] != 0:
                    if D[i][i] == 0:
                        for k in range(rows): D[k][i], D[k][j] = D[k][j], D[k][i]
                        for k in range(cols): T[k][i], T[k][j] = T[k][j], T[k][i]
                    else:
                        q = int(D[i][j] / D[i][i])
                        sub_col(j, i, q)
                        if D[i][j] != 0:
                            for k in range(rows): D[k][i], D[k][j] = D[k][j], D[k][i]
                            for k in range(cols): T[k][i], T[k][j] = T[k][j], T[k][i]
                    changed = True

    b_prime = [0] * rows
    for i in range(rows):
        sum_val = 0
        for j in range(rows):
            sum_val = to_u64(sum_val + to_u64(S[i][j]) * to_u64(b[j]))
        b_prime[i] = to_i64(sum_val)

    x_prime = [0] * cols
    for i in range(min(rows, cols)):
        d = D[i][i]
        bp = b_prime[i]
        if d == 0:
            if bp != 0: return None
            else: x_prime[i] = random.randint(-2**63, 2**63 - 1)
            continue
        k = ctz(d)
        c = to_u64(d) >> k
        b_uint = to_u64(bp)
        if k > 0 and (b_uint & ((1 << k) - 1)) != 0: return None
        b_new = b_uint >> k
        c_inv = modInverse_odd(c)
        x_prime[i] = to_i64(to_u64(b_new * c_inv))

    for i in range(min(rows, cols), cols):
        if random.randint(0, 99) >= intensity:
            x_prime[i] = 0
        else:
            x_prime[i] = random.randint(-2**63, 2**63 - 1)

    x = [0] * cols
    for i in range(cols):
        sum_val = 0
        for j in range(cols):
            sum_val = to_u64(sum_val + to_u64(T[i][j]) * to_u64(x_prime[j]))
        x[i] = to_i64(sum_val)
    return x

def format_expression(x_sol, perm):
    terms = []
    for j in range(len(basis_pool)):
        coef = x_sol[j]
        if coef == 0: continue
        basis_name = basis_pool[perm[j]]["name"]
        if basis_name == "0": continue
        elif basis_name == "-1":
            terms.append(f"{'+' if to_i64(-coef)>0 else '-'} {abs(to_i64(-coef))}")
        else:
            terms.append(f"{'+' if coef>0 else '-'} {abs(coef)} * {basis_name}")
    expr_str = " ".join(terms)
    return expr_str[2:] if expr_str.startswith("+ ") else expr_str

def build_z3_expr(x_sol, perm, z3_x, z3_y, bit_width):
    expr = BitVecVal(0, bit_width)
    for j in range(len(x_sol)):
        coef = x_sol[j]
        if coef == 0: continue
        basis_name = basis_pool[perm[j]]["name"]
        basis_expr = get_z3_basis(z3_x, z3_y, basis_name, bit_width)
        c_val = BitVecVal(coef & ((1 << bit_width) - 1), bit_width)
        expr = expr + (c_val * basis_expr)
    return expr

def run_benchmark():
    print(f"\n{CYAN}{BOLD}======================================================================{RESET}")
    print(f"{CYAN}{BOLD}         AmazeLLVM Z3 MBA Constraint Hardness Benchmark               {RESET}")
    print(f"{CYAN}{BOLD}======================================================================{RESET}\n")

    operators = [
        {"name": "42", "b": [42, 42, 42, 42], "z3_op": lambda x,y: 42},
        {"name": "x + y", "b": [0, 1, 1, 2], "z3_op": lambda x,y: x + y},
        {"name": "x - y", "b": [0, -1, 1, 0], "z3_op": lambda x,y: x - y},
        {"name": "x ^ y", "b": [0, 1, 1, 0], "z3_op": lambda x,y: x ^ y},
    ]

    A = compute_basis_matrix()
    cols = len(basis_pool)

    for op in operators:
        print(f"{BOLD}Target Expression:{RESET} {YELLOW}{op['name']}{RESET}")
        
        # 1. Generate MBA Coefficients
        x_sol, perm = None, None
        for _ in range(10):
            perm = list(range(cols))
            random.shuffle(perm)
            A_shuffled = [[A[r][perm[c]] for c in range(cols)] for r in range(4)]
            x_sol = solve(A_shuffled, op["b"], intensity=50)
            if x_sol is not None: break
        
        if x_sol is None:
            print("Failed to generate MBA. Try again.")
            continue

        print(f"{BOLD}Generated MBA Inorder Representation:{RESET}")
        print(f"  {GREEN}{format_expression(x_sol, perm)}{RESET}\n")

        print(f"{BOLD}Running Z3 SMT Benchmark across varying bit-widths...{RESET}")
        bit_widths = [8, 10, 12, 14, 16]
        timeout_sec = 60  # 1 minute timeout per test

        for bw in bit_widths:
            x, y = BitVecs('x y', bw)
            
            # Construct obfuscated Z3 expression
            expr_obf = build_z3_expr(x_sol, perm, x, y, bw)
            expr_orig = op["z3_op"](x, y)
            
            # Simplify expression to allow Z3 to fold constants
            expr_simplified = simplify(expr_obf)
            if bw == bit_widths[0]:
                print(f"  {PURPLE}Z3 Simplified Formula (Sample from {bw}-bit):{RESET}")
                print(f"  {expr_simplified}\n")
            
            # Check for equivalence: (expr_obf != expr_orig) should be UNSAT
            s = SolverFor("QF_BV")
            t = Then('simplify', 'solve-eqs', 'bit-blast', 'sat')
            s = t.solver()
            s.add(expr_simplified != expr_orig)
            
            print(f"  {BOLD}Testing {bw}-bit...{RESET} ", end="", flush=True)
            
            start = time.time()
            s.set("timeout", timeout_sec * 1000)
            result = s.check()
            elapsed = time.time() - start
            
            if result == z3.unsat:
                print(f"{GREEN}Solved (UNSAT){RESET} in {elapsed:.2f}s")
            elif result == z3.unknown:
                print(f"\033[1;31mTIMEOUT{RESET} (> {timeout_sec}s)")
                print("  Stopping further bit-widths.")
                break
            else:
                print(f"\033[1;31mFAIL{RESET} (Found Counterexample!)")
                break
        print("-" * 70)

if __name__ == "__main__":
    run_benchmark()
