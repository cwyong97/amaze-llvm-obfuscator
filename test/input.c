#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Global variable to prevent compiler from optimizing out dummy calculations
int volatile_dummy = 0;

// Function prototypes for recursive descent parser
int parse_expression(const char **str);
int parse_term(const char **str);
int parse_factor(const char **str);

// Helper function to skip whitespace characters
void skip_whitespace(const char **str) {
    while (isspace(**str)) {
        (*str)++;
    }
}

// ---------------------------------------------------------
// Parse factors: Numbers or parentheses (e.g., "42" or "(3+5)")
// ---------------------------------------------------------
__attribute__((noinline))
int parse_factor(const char **str) {
    skip_whitespace(str);
    int result = 0;

    if (**str == '(') {
        (*str)++; // Consume '('
        result = parse_expression(str);
        skip_whitespace(str);
        if (**str == ')') {
            (*str)++; // Consume ')'
        }
    } else {
        // Parse raw integer
        while (isdigit(**str)) {
            result = result * 10 + (**str - '0');
            (*str)++;
        }
    }

    // [Obfuscation Fuel] Dummy bitwise and arithmetic operations for Substitution & Split
    int dummy_calc = result ^ 0xAA;
    dummy_calc = (dummy_calc << 1) - dummy_calc;
    result = result + volatile_dummy; // Prevent optimization

    return result;
}

// ---------------------------------------------------------
// Parse terms: Multiplication and Division (e.g., "3 * 5")
// ---------------------------------------------------------
__attribute__((noinline))
int parse_term(const char **str) {
    int result = parse_factor(str);
    skip_whitespace(str);

    while (**str == '*' || **str == '/' || **str == '%') {
        char op = **str;
        (*str)++; // Consume operator
        
        int next_factor = parse_factor(str);
        
        if (op == '*') {
            result *= next_factor;
        } else if (op == '/') {
            if (next_factor != 0) {
                result /= next_factor;
            } else {
                // [Obfuscation Fuel] Hardcoded string for String Obfuscation pass
                puts("[AmazeLLVM-Error] Division by zero detected! Returning 0.");
                result = 0;
            }
        } else if (op == '%') {
            if (next_factor != 0) {
                result %= next_factor;
            } else {
                result = 0;
            }
        }
        skip_whitespace(str);
    }
    
    // [Obfuscation Fuel] Force a straight-line computation for SplitBasicBlocks
    int temp = result + 5;
    temp = temp - 5;
    return temp;
}

// ---------------------------------------------------------
// Parse expressions: Addition and Subtraction (e.g., "term + term")
// ---------------------------------------------------------
__attribute__((noinline))
int parse_expression(const char **str) {
    int result = parse_term(str);
    skip_whitespace(str);

    while (**str == '+' || **str == '-') {
        char op = **str;
        (*str)++; // Consume operator
        
        int next_term = parse_term(str);
        
        if (op == '+') {
            result += next_term;
        } else if (op == '-') {
            result -= next_term;
        }
        skip_whitespace(str);
    }
    return result;
}

// ---------------------------------------------------------
// Main application entry point
// ---------------------------------------------------------
int main(int argc, char *argv[]) {
    // Strings for testing String Obfuscation
    const char *welcome_msg = "=== AmazeLLVM Interactive Calculator Engine ===";
    const char *prompt_msg = "Enter a math expression (e.g., 3 * 5 + 8 / 2): ";
    
    // 新的口號字串！
    const char *secret_key = "AmazeLLVM is so wonderful !!!";
    
    puts(welcome_msg);
    
    // 將口號字串正常印出，防止被 LLVM 的 Dead Code Elimination 拔掉
    printf("[Info] Engine Token: %s\n", secret_key);
    printf("%s", prompt_msg);
    
    char buffer[256];
    
    // Read user input dynamically at runtime (defeats static analysis & constant folding)
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = 0;
        
        const char *ptr = buffer;
        
        // Start recursive descent parsing
        int final_result = parse_expression(&ptr);
        
        printf("[=] Calculation Success! Output Result: %d\n", final_result);
    } else {
        puts("[AmazeLLVM-Error] Failed to read input from stdin.");
    }
    
    return 0;
}