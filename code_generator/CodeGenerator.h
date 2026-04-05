#pragma once
#include "parser/ast/ModuleNode.h"

// Abstract interface for all code generation backends.
// Implementations: LLVMCodeGenerator (native), IRBuilder (custom IR / WASM).
class CodeGenerator {
public:
    virtual ~CodeGenerator() = default;
    virtual void generate_code(ModuleNode &module) = 0;
};
