// basis.h
#pragma once // 防止重複引入

#include <vector>
#include <string>
#include <functional>
#include <cstdint> // 提供 uint64_t

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Constants.h"

using llvm::IRBuilder;
using llvm::Value;
using llvm::ConstantInt;

struct Basis {
    std::string name;
    std::function<uint64_t(uint64_t, uint64_t)> op;
    std::function<Value*(IRBuilder<>&, Value*, Value*)> genIR;
};

inline std::vector<Basis> basis_pool = {
    {"0", 
        [](uint64_t x, uint64_t y) { return 0ULL; },
        [](IRBuilder<>& b, Value* x, Value* y) { return ConstantInt::get(x->getType(), 0); }},
    
    {"(x & y)", 
        [](uint64_t x, uint64_t y) { return x & y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateAnd(x, y); }},
    
    {"(x & ~y)", 
        [](uint64_t x, uint64_t y) { return x & ~y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateAnd(x, b.CreateNot(y)); }},
    
    {"x", 
        [](uint64_t x, uint64_t y) { return x; },
        [](IRBuilder<>& b, Value* x, Value* y) { return x; }},
    
    {"(~x & y)", 
        [](uint64_t x, uint64_t y) { return ~x & y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateAnd(b.CreateNot(x), y); }},
    
    {"y", 
        [](uint64_t x, uint64_t y) { return y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return y; }},
    
    {"(x ^ y)", 
        [](uint64_t x, uint64_t y) { return x ^ y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateXor(x, y); }},
    
    {"(x | y)", 
        [](uint64_t x, uint64_t y) { return x | y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateOr(x, y); }},
    
    {"(~(x | y))", 
        [](uint64_t x, uint64_t y) { return ~(x | y); },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateNot(b.CreateOr(x, y)); }},
    
    {"(~(x ^ y))", 
        [](uint64_t x, uint64_t y) { return ~(x ^ y); },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateNot(b.CreateXor(x, y)); }},
    
    {"(~y)", 
        [](uint64_t x, uint64_t y) { return ~y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateNot(y); }},
    
    {"(x | ~y)", 
        [](uint64_t x, uint64_t y) { return x | ~y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateOr(x, b.CreateNot(y)); }},
    
    {"(~x)", 
        [](uint64_t x, uint64_t y) { return ~x; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateNot(x); }},
    
    {"(~x | y)", 
        [](uint64_t x, uint64_t y) { return ~x | y; },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateOr(b.CreateNot(x), y); }},
    
    {"(~(x & y))", 
        [](uint64_t x, uint64_t y) { return ~(x & y); },
        [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateNot(b.CreateAnd(x, y)); }},
    
    {"-1", 
        [](uint64_t x, uint64_t y) { return -1ULL; },
        [](IRBuilder<>& b, Value* x, Value* y) { return ConstantInt::get(x->getType(), -1ULL); }},
    // {"-(~x + 1)", 
    //     [](uint64_t x, uint64_t y) { return -(~x + 1); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateNeg(b.CreateAdd(b.CreateNot(x), ConstantInt::get(x->getType(), 1))); }},
    //{
    //     "x<<1",   //這個基底容易overflow，導致數值錯誤，所以先拿掉
    //     [](uint64_t x, uint64_t y) { return x << 1; },
    //     [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateShl(x, ConstantInt::get(x->getType(), 1)); }
    // },
    
    //以下這些基底雖然也沒問題，但目前先不加入，保持基底池簡單一點，等之後需要更多變化再說
    // {"x+y", 
    //     [](uint64_t x, uint64_t y) { return x + y; },
    //     [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateAdd(x, y); }},
    
    // {"x-y", 
    //     [](uint64_t x, uint64_t y) { return x - y; },
    //     [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateSub(x, y); }},
    
    // {"y-x", 
    //     [](uint64_t x, uint64_t y) { return y - x; },
    //     [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateSub(y, x); }},
    

    // {"(x&y)|(~x&~y)", 
    //     [](uint64_t x, uint64_t y) { return (x & y) | (~x & ~y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateOr(b.CreateAnd(x, y), b.CreateAnd(b.CreateNot(x), b.CreateNot(y))); 
    //     }},
    
    // {"(x|y)&(~x|~y)", 
    //     [](uint64_t x, uint64_t y) { return (x | y) & (~x | ~y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateAnd(b.CreateOr(x, y), b.CreateOr(b.CreateNot(x), b.CreateNot(y))); 
    //     }},
    
    // {"(x&(y^(x^y)))", 
    //     [](uint64_t x, uint64_t y) { return (x & (y ^ (x ^ y))); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateAnd(x, b.CreateXor(y, b.CreateXor(x, y))); 
    //     }},
    

    // {"(x&y)+(x^y)", 
    //     [](uint64_t x, uint64_t y) { return (x & y) + (x ^ y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateAdd(b.CreateAnd(x, y), b.CreateXor(x, y)); 
    //     }},
    
    // {"x&(x|y)", 
    //     [](uint64_t x, uint64_t y) { return x & (x | y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateAnd(x, b.CreateOr(x, y)); 
    //     }},
    
    // {"x|(x&y)", 
    //     [](uint64_t x, uint64_t y) { return x | (x & y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateOr(x, b.CreateAnd(x, y)); 
    //     }},
    
    // {"(x&~y)|(y&~x)", 
    //     [](uint64_t x, uint64_t y) { return (x & ~y) | (y & ~x); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateOr(
    //             b.CreateAnd(x, b.CreateNot(y)), 
    //             b.CreateAnd(y, b.CreateNot(x))
    //         ); 
    //     }},
    
    // {"x^(x&y)", 
    //     [](uint64_t x, uint64_t y) { return x ^ (x & y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateXor(x, b.CreateAnd(x, y)); 
    //     }},
    
    // {"y^(x&y)", 
    //     [](uint64_t x, uint64_t y) { return y ^ (x & y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateXor(y, b.CreateAnd(x, y)); 
    //     }},
    
    // {"x^(x|y)", 
    //     [](uint64_t x, uint64_t y) { return x ^ (x | y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateXor(x, b.CreateOr(x, y)); 
    //     }},
    
    // {"(x|y)^(x&y)", 
    //     [](uint64_t x, uint64_t y) { return (x | y) ^ (x & y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateXor(b.CreateOr(x, y), b.CreateAnd(x, y)); 
    //     }},
    
    // {"~((~x&~y)|(x&y))", 
    //     [](uint64_t x, uint64_t y) { return ~((~x & ~y) | (x & y)); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateNot(b.CreateOr(
    //             b.CreateAnd(b.CreateNot(x), b.CreateNot(y)), 
    //             b.CreateAnd(x, y)
    //         )); 
    //     }},
    
    // {"x+(x&y)", 
    //     [](uint64_t x, uint64_t y) { return x + (x & y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateAdd(x, b.CreateAnd(x, y)); 
    //     }},
    
    // {"y+(x&y)", 
    //     [](uint64_t x, uint64_t y) { return y + (x & y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateAdd(y, b.CreateAnd(x, y)); 
    //     }},

};

    
    // {"x>>1", 
    //     [](uint64_t x, uint64_t y) { return x >> 1; },
    //     [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateLShr(x, ConstantInt::get(x->getType(), 1)); }},
    // {"x*y", 
    //     [](uint64_t x, uint64_t y) { return x * y; },
    //     [](IRBuilder<>& b, Value* x, Value* y) { return b.CreateMul(x, y); }},
    
    // {"((x&y)<<1)|(x^y)", 
    //     [](uint64_t x, uint64_t y) { return ((x & y) << 1) | (x ^ y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateOr(
    //             b.CreateShl(b.CreateAnd(x, y), ConstantInt::get(x->getType(), 1)), 
    //             b.CreateXor(x, y)
    //         ); 
    //     }},
    
    
    // {"(x+y)&(x|y)", 
    //     [](uint64_t x, uint64_t y) { return (x + y) & (x | y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateAnd(b.CreateAdd(x, y), b.CreateOr(x, y)); 
    //     }},
    
    // {"(x+y)|(x&y)", 
    //     [](uint64_t x, uint64_t y) { return (x + y) | (x & y); },
    //     [](IRBuilder<>& b, Value* x, Value* y) { 
    //         return b.CreateOr(b.CreateAdd(x, y), b.CreateAnd(x, y)); 
    //     }}