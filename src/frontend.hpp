/* LLGO Frontend IR Definition */

#pragma once
#include <vector>
#include <string>

namespace llgo
{
    namespace frontend
    {
        // Type tags
        enum class Type
        {
            i2, i4, i8, i16, i32, i64, i128, i256,
            u1, u2, u4, u8, u16, u32, u64, u128, u256,
            const_u8,   // string literal (const u8[])
            boolean,    // logical type
            arr,        // requires metadata
            ptr         // requires metadata
        };

        // Type metadata (not part of the IR itself)
        struct TypeInfo
        {
            Type base;
            size_t bitWidth;     // for ints/floats
            Type elementKind;    // for arr/ptr
            size_t arraySize;    // only for arr
        };

        // SSA value
        struct Value
        {
            Type type;
            std::string name; // SSA identifier
        };

        // Instruction kinds
        enum class InstrKind
        {
            Add, Sub, Mul, Div, Mod,
            Icmp, Fcmp,
            Load, Store, Gep,
            Phi,
            Br, CondBr, Ret,
            Call,
            ConstInt, ConstFloat, ConstString,
            Undef
        };

        // IR instruction
        struct Instr
        {
            InstrKind kind;
            Type resultType;                 // type of produced value (if any)
            std::string resultName;          // SSA name of produced value ("" if none)

            std::vector<Value> operands;     // SSA operands

            std::string control;             // control dependency
            std::string memory;              // memory/effect dependency

            std::string targetTrue;          // CondBr
            std::string targetFalse;         // CondBr
            std::string target;              // Br
        };

        // Basic block
        struct Block
        {
            std::string name;
            std::vector<Instr> instructions;
        };

        // Function
        struct Function
        {
            std::string name;
            std::vector<Value> params;
            Type returnType;
            std::vector<Block> blocks;
        };

        // Module
        struct Module
        {
            std::vector<Function> functions;
        };

    }
} // namespace llgo
