import angr
import claripy
import sys
import time
import logging

def run_angr(binary_path, timeout_sec):
    # Enable internal angr logging to see the UNSAT drops
    logging.getLogger('angr.sim_manager').setLevel(logging.DEBUG)
    logging.getLogger('angr.state_plugins.solver').setLevel(logging.DEBUG)
    
    print(f"[*] Starting advanced penetration testing on {binary_path}...")
    print(f"[*] Timeout set to {timeout_sec} seconds. Full send engaged.")
    
    # Load the binary
    project = angr.Project(binary_path, auto_load_libs=False)
    from angr.procedures.libc.puts import puts
    if project.loader.main_object.pic:
        plt_puts = 0x400000 + 0x1030
    else:
        plt_puts = 0x1030
        
    print(f"[*] Hooking PLT puts at {hex(plt_puts)}")
    project.hook(plt_puts, puts())
    # Create 4 symbolic bytes for the integer input
    sym_input = claripy.BVS('sym_input', 4 * 8)
    
    # Enable advanced options for orthodox penetration testing:
    # 1. Unicorn Engine: Massive concrete execution speedup
    # 2. Symbolic Fill: Counter BogusControlFlow volatile memory injections 
    #    (forces angr to model the opaque predicates mathematically instead of dropping the path)
    extras = {
        angr.options.SYMBOL_FILL_UNCONSTRAINED_MEMORY,
        angr.options.SYMBOL_FILL_UNCONSTRAINED_REGISTERS,
        angr.options.LAZY_SOLVES,
    }
    
    # Setup state with full_init_state to ensure .init_array constructors run
    state = project.factory.entry_state(
        args=[binary_path], 
        stdin=sym_input,
        add_options=extras
    )
    
    # --- ADDED: Debug Hook to show exactly where Z3 hangs ---
    # We hook the 'irsb' (block) execution. It will print the address right before
    # angr attempts to symbolically evaluate the block. When it hits the MBA/BCF block,
    # the terminal will freeze immediately after printing the address.
    global_start_time = time.time()
    def debug_block(state):
        offset = time.time() - global_start_time
        print(f"[DEBUG] [{offset:06.3f}s] Symbolically evaluating block at {hex(state.addr)}...", flush=True)
    
    state.inspect.b('irsb', when=angr.BP_BEFORE, action=debug_block)
    # --------------------------------------------------------
    
    simgr = project.factory.simulation_manager(state)
    
    # Enable veritesting (angr's strongest weapon against path explosion)
    # It attempts to merge states statically where possible.
    # Note: This is intentionally commented out for AmazeLLVM testing.
    # When enabled, the extreme complexity and structure of the obfuscated LLVM IR
    # (specifically unconstrained memory blocks from BCF) causes the native VEX engine
    # to crash (SimEngineError: No bytes in memory for block). This indicates that the
    # obfuscation breaks the stability boundaries of angr's core path-merging engine.
    # simgr.use_technique(angr.exploration_techniques.Veritesting())
    
    start_time = time.time()
    
    def is_successful(state):
        stdout_output = state.posix.dumps(1)
        return b"SUCCESS" in stdout_output

    def is_failed(state):
        stdout_output = state.posix.dumps(1)
        return b"FAIL" in stdout_output

    class TimeoutException(Exception):
        pass

    def step_func(simgr):
        elapsed = time.time() - start_time
        if elapsed > timeout_sec:
            raise TimeoutException(f"Timeout reached ({elapsed:.2f}s)")
        
        # Print progress every 5 seconds
        if int(elapsed) > 0 and int(elapsed) % 5 == 0 and not hasattr(simgr, 'last_print_time'):
            print(f"[*] elapsed: {elapsed:.2f}s | active states: {len(simgr.active)}")
            setattr(simgr, 'last_print_time', int(elapsed))
        elif hasattr(simgr, 'last_print_time') and int(elapsed) % 5 == 0 and int(elapsed) != getattr(simgr, 'last_print_time'):
            print(f"[*] elapsed: {elapsed:.2f}s | active states: {len(simgr.active)}")
            setattr(simgr, 'last_print_time', int(elapsed))
            
        return simgr
        
    try:
        # Full send exploration
        print(f"[*] Initializing exploration manager...")
        simgr.explore(find=is_successful, avoid=is_failed, step_func=step_func)
        end_time = time.time()
        
        if simgr.found:
            found_state = simgr.found[0]
            solution = found_state.solver.eval(sym_input, cast_to=int)
            print(f"\n[+] Exploration succeeded in {end_time - start_time:.2f} seconds.")
            print(f"[+] Found valid input: {solution}")
        else:
            avoid_count = len(simgr.stashes.get('avoid', []))
            print(f"\n[-] Exploration timed out or failed to converge in {end_time - start_time:.2f} seconds.")
            print(f"[-] Final state count - Active: {len(simgr.active)}, Deadended: {len(simgr.deadended)}, Avoid: {avoid_count}")
            
    except (TimeoutException, KeyboardInterrupt) as e:
        print(f"\n[-] Exploration timed out: {e}")
        print(f"[-] Active states at timeout: {len(simgr.active)}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 angr_test.py <binary>")
        sys.exit(1)
    
    # We set default timeout to 120 seconds to see the state explosion happen live
    run_angr(sys.argv[1], 6000)
