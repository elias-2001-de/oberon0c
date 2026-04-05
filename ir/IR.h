#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <memory>

// ── Types ─────────────────────────────────────────────────────────────────

struct IRType {
    enum class Kind { Int1, Int32, Int64, Void, Ptr, Array, Record };
    Kind kind = Kind::Void;
    int  array_size = 0;
    std::shared_ptr<IRType> elem_type;
    std::string record_name;

    static IRType int1()  { IRType t; t.kind = Kind::Int1;  return t; }
    static IRType int32() { IRType t; t.kind = Kind::Int32; return t; }
    static IRType int64() { IRType t; t.kind = Kind::Int64; return t; }
    static IRType void_() { IRType t; t.kind = Kind::Void;  return t; }
    static IRType ptr()   { IRType t; t.kind = Kind::Ptr;   return t; }
    static IRType array_(int n, IRType elem) {
        IRType t;
        t.kind       = Kind::Array;
        t.array_size = n;
        t.elem_type  = std::make_shared<IRType>(std::move(elem));
        return t;
    }
    static IRType record_(std::string name) {
        IRType t;
        t.kind        = Kind::Record;
        t.record_name = std::move(name);
        return t;
    }

    std::string str() const {
        switch (kind) {
        case Kind::Int1:   return "i1";
        case Kind::Int32:  return "i32";
        case Kind::Int64:  return "i64";
        case Kind::Void:   return "void";
        case Kind::Ptr:    return "ptr";
        case Kind::Array:  return "[" + std::to_string(array_size) + " x " + elem_type->str() + "]";
        case Kind::Record: return "%" + record_name;
        }
        return "unknown_type";
    }

    std::string zero_init() const {
        switch (kind) {
        case Kind::Int1:
        case Kind::Int32:
        case Kind::Int64:  return "0";
        case Kind::Array:
        case Kind::Record: return "zeroinitializer";
        default:           return "0";
        }
    }
};

// ── Instructions ──────────────────────────────────────────────────────────

struct IRAlloca  { std::string dst; IRType type; };
struct IRLoad    { std::string dst; IRType type; std::string src; };
struct IRStore   { IRType type; std::string src; std::string dst; };
struct IRGEP     { std::string dst; IRType base_type; std::string src;
                   std::vector<std::string> indices; };
struct IRBinOp   { std::string dst; std::string op; IRType type;
                   std::string lhs; std::string rhs; };
struct IRICmp    { std::string dst; std::string pred; IRType type;
                   std::string lhs; std::string rhs; };
struct IRBr      { std::string label; };
struct IRCondBr  { std::string cond; std::string true_lbl; std::string false_lbl; };
struct IRCall    { std::optional<std::string> dst; IRType ret_type; std::string fn;
                   std::vector<std::pair<IRType, std::string>> args; };
struct IRRetVoid {};
struct IRRetI32  { int val; };

using IRInstr = std::variant<IRAlloca, IRLoad, IRStore, IRGEP, IRBinOp, IRICmp,
                              IRBr, IRCondBr, IRCall, IRRetVoid, IRRetI32>;

inline std::string instr_str(const IRInstr &instr)
{
    return std::visit([](const auto &i) -> std::string {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, IRAlloca>) {
            return "  %" + i.dst + " = alloca " + i.type.str();
        } else if constexpr (std::is_same_v<T, IRLoad>) {
            return "  %" + i.dst + " = load " + i.type.str() + ", ptr " + i.src;
        } else if constexpr (std::is_same_v<T, IRStore>) {
            return "  store " + i.type.str() + " " + i.src + ", ptr " + i.dst;
        } else if constexpr (std::is_same_v<T, IRGEP>) {
            std::string s = "  %" + i.dst + " = getelementptr " +
                            i.base_type.str() + ", ptr " + i.src;
            for (auto &idx : i.indices) s += ", i64 " + idx;
            return s;
        } else if constexpr (std::is_same_v<T, IRBinOp>) {
            return "  %" + i.dst + " = " + i.op + " " + i.type.str() +
                   " " + i.lhs + ", " + i.rhs;
        } else if constexpr (std::is_same_v<T, IRICmp>) {
            return "  %" + i.dst + " = icmp " + i.pred + " " + i.type.str() +
                   " " + i.lhs + ", " + i.rhs;
        } else if constexpr (std::is_same_v<T, IRBr>) {
            return "  br label %" + i.label;
        } else if constexpr (std::is_same_v<T, IRCondBr>) {
            return "  br i1 " + i.cond + ", label %" + i.true_lbl +
                   ", label %" + i.false_lbl;
        } else if constexpr (std::is_same_v<T, IRCall>) {
            std::string s = "  ";
            if (i.dst) s += "%" + *i.dst + " = ";
            s += "call " + i.ret_type.str() + " @" + i.fn + "(";
            for (size_t j = 0; j < i.args.size(); j++) {
                if (j > 0) s += ", ";
                s += i.args[j].first.str() + " " + i.args[j].second;
            }
            s += ")";
            return s;
        } else if constexpr (std::is_same_v<T, IRRetVoid>) {
            return "  ret void";
        } else if constexpr (std::is_same_v<T, IRRetI32>) {
            return "  ret i32 " + std::to_string(i.val);
        }
        return "  ; unknown_instr";
    }, instr);
}

// ── Basic Block ───────────────────────────────────────────────────────────

struct IRBasicBlock {
    std::string          label;
    std::vector<IRInstr> instrs;

    std::string str() const {
        std::string s = label + ":\n";
        for (auto &instr : instrs) s += instr_str(instr) + "\n";
        return s;
    }
};

// ── Function ──────────────────────────────────────────────────────────────

struct IRFunction {
    std::string name;
    IRType return_type;
    std::vector<std::pair<IRType, std::string>> params;
    std::vector<IRBasicBlock>                   blocks;

    std::string str() const {
        std::string s = "define " + return_type.str() + " @" + name + "(";
        for (size_t i = 0; i < params.size(); i++) {
            if (i > 0) s += ", ";
            s += params[i].first.str() + " %" + params[i].second;
        }
        s += ") {\n";
        for (auto &block : blocks) s += block.str();
        s += "}\n";
        return s;
    }
};

// ── Global Variable ───────────────────────────────────────────────────────

struct IRGlobal {
    std::string name;
    IRType      type;
    std::string init;

    std::string str() const {
        return "@" + name + " = internal global " + type.str() + " " + init;
    }
};

// ── Type Definition (named struct for records) ────────────────────────────

struct IRTypeDef {
    std::string          name;
    std::vector<IRType>  field_types;

    std::string str() const {
        std::string s = "%" + name + " = type {";
        for (size_t i = 0; i < field_types.size(); i++) {
            if (i > 0) s += ", ";
            s += field_types[i].str();
        }
        return s + "}";
    }
};

// ── Module ────────────────────────────────────────────────────────────────

struct IRModule {
    std::string              name;
    std::vector<IRTypeDef>   type_defs;
    std::vector<IRGlobal>    globals;
    std::vector<IRFunction>  functions;

    std::string str() const {
        std::string s = "; ModuleID = '" + name + "'\n";
        s += "source_filename = \"" + name + "\"\n\n";
        for (auto &td : type_defs) s += td.str() + "\n";
        if (!type_defs.empty()) s += "\n";
        for (auto &g : globals)   s += g.str() + "\n";
        if (!globals.empty()) s += "\n";
        for (auto &f : functions) s += f.str() + "\n";
        return s;
    }
};
