# Compiler
EMCC = emcc

# Output
OUT = wasm_lib.js

# Include Paths
INCLUDES = -I. -I/home/elias/code/emsdk/cache/sysroot/include -I./boost

# Compiler Flags (Only for Compilation)
CXXFLAGS = -O2 -Wall $(INCLUDES)

# Linker Flags (Only for Linking) 
LDFLAGS = -s MODULARIZE=1 -s EXPORT_ES6=0 -s ENVIRONMENT=web \
          -s EXPORTED_FUNCTIONS="['_scanner', '_parse', '_check', '_compile', '_compile_wat']" \
          -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "UTF8ToString"]' \
          -s TOTAL_STACK=64MB -s ALLOW_MEMORY_GROWTH=1 -L./boost/stage/lib/

# Source Files
SCANNER_SRC = scanner/Scanner.cpp scanner/Token.cpp scanner/UndefinedToken.cpp \
              scanner/LiteralToken.cpp scanner/IdentToken.cpp

PARSER_SRC = parser/ast/base_blocks/ExpressionNode.cpp parser/ast/base_blocks/IdentNode.cpp \
             parser/ast/base_blocks/IntNode.cpp parser/ast/base_blocks/SelectorNode.cpp \
             parser/ast/declarations/ArrayTypeNode.cpp parser/ast/declarations/DeclarationsNode.cpp \
             parser/ast/declarations/ProcedureDeclarationNode.cpp parser/ast/declarations/RecordTypeNode.cpp \
             parser/ast/declarations/TypeNode.cpp parser/ast/statements/AssignmentNode.cpp \
             parser/ast/statements/IfStatementNode.cpp parser/ast/statements/ProcedureCallNode.cpp \
             parser/ast/statements/RepeatStatementNode.cpp parser/ast/statements/StatementNode.cpp \
             parser/ast/statements/StatementSequenceNode.cpp parser/ast/statements/WhileStatementNode.cpp \
             parser/ast/ModuleNode.cpp parser/ast/Node.cpp parser/ast/NodeVisitor.cpp parser/Parser.cpp \
             parser/ast/JsonVisitor.cpp

CHECKER_SRC = semantic_checker/ScopeTable.cpp \
              semantic_checker/SemanticChecker.cpp \
              semantic_checker/SymbolTable.cpp

UTIL_SRC = util/Logger.cpp util/panic.cpp

IR_SRC = ir/IRBuilder.cpp

CODEGEN_SRC = code_generator/WATCodeGenerator.cpp

MAIN_SRC = wasm_lib.cpp

# Object Files
OBJS = $(SCANNER_SRC:.cpp=.o) $(PARSER_SRC:.cpp=.o) $(UTIL_SRC:.cpp=.o) $(CHECKER_SRC:.cpp=.o) $(IR_SRC:.cpp=.o) $(CODEGEN_SRC:.cpp=.o) $(MAIN_SRC:.cpp=.o)

# Build Rule
all: $(OUT)

$(OUT): $(OBJS)
	$(EMCC) $(OBJS) -o $(OUT) $(LDFLAGS)

# Compile C++ source files to object files (Only Uses CXXFLAGS)
%.o: %.cpp
	$(EMCC) -c $< -o $@ $(CXXFLAGS)

# Clean
clean:
	rm -f $(OBJS) $(OUT) $(OUT:.js=.wasm)
