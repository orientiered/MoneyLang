#ifndef BACKEND_H
#define BACKEND_H

#include <stdint.h>
#include <stdlib.h>

#include "utils.h"
#include "nameTable.h"
#include "Context.h"

typedef struct LocalVar_t {
    int id;
    unsigned address;
} LocalVar_t;

typedef struct LocalsStack_t {
    LocalVar_t *vars;
    size_t size;
    size_t capacity;
} LocalsStack_t;

typedef enum BackendStatus_t {
    BACKEND_SUCCESS,
    BACKEND_AST_ERROR,
    BACKEND_FILE_ERROR,
    BACKEND_WRITE_ERROR,
    BACKEND_TYPE_ERROR,
    BACKEND_SCOPE_ERROR,
    BACKEND_ERROR
} BackendStatus_t;

typedef enum BackendMode_t {
    BACKEND_SIMPLE = 0,
    BACKEND_TAXES = 1
} BackendMode_t;

/* ============= Intermediate representation ======================== */
// IR for stack-based operations

typedef enum {
    IR_NOP, ///< use for commentaries
    // binary arithmetical operations
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    // unary arithmetical operations
    IR_SQRT,
    // comparison operations
    IR_CMPLT,
    IR_CMPGT,
    IR_CMPLE,
    IR_CMPGE,
    IR_CMPEQ,
    IR_CMPNEQ,
    // assign
    IR_ASSIGN, //? Maybe redundant
    IR_PUSH,
    IR_POP,
    // control flow
    IR_JMP,
    IR_JZ,
    IR_CALL,
    IR_RET,
    // IR_ENTER,
    // IR_LEAVE,
    IR_EXIT

} IRNodeType_t;

enum IRPushPopType {
    PUSH_IMM,
    PUSH_MEM,
    PUSH_REG,
    POP_MEM
};

typedef struct {
    int64_t offset;
} IRaddr_t;

typedef struct {
    IRNodeType_t    type;
    union {
        double dval;
        IRaddr_t addr;
    };
    enum IRPushPopType extra;

    const char *comment;
} IRNode_t;

typedef struct {
    IRNode_t *nodes;
    uint32_t size;
    uint32_t capacity;

    char *comments;
    char *commentPtr;
} IR_t;

IR_t IRCtor(size_t capacity);
void IRDtor(IR_t *ir);

const size_t IR_MAX_SIZE = 4096;
const size_t IR_MAX_COMMENTS_LEN = 16384;
/* ====================================================================== */

typedef struct BackendContext_t {
    const char *inputFileName;
    const char *outputFileName;
    const char *text;

    NameTable_t nameTable;

    MemoryArena_t treeMemory;
    Node_t *tree;
    IR_t IR;

    int mode;

    LocalsStack_t stk;
    bool inFunction;
    int operatorCounter;
    int ifCounter;
    int whileCounter;
} Backend_t;

BackendStatus_t convertASTtoIR(BackendContext_t *backend, Node_t *ast);

BackendStatus_t LocalsStackInit(LocalsStack_t *stk, size_t capacity);
BackendStatus_t LocalsStackDelete(LocalsStack_t *stk);

enum ScopeType {
    FUNC_SCOPE = -1,
    NORMAL_SCOPE = -2
};

BackendStatus_t LocalsStackPush(LocalsStack_t *stk, int id);
//searches variable in stack and prints it
BackendStatus_t LocalsStackSearchPrint(LocalsStack_t *stk, int id, Backend_t *backend, FILE *file);
BackendStatus_t LocalsStackPopScope(LocalsStack_t *stk);
BackendStatus_t LocalsStackInitScope(LocalsStack_t *stk, enum ScopeType scope);

const size_t LOCALS_STACK_SIZE = 128;
const size_t PROCESSOR_RAM_SIZE = 16384;


const double TAX_COEFF = 0.8;

/// @brief Initialize frontend context
BackendStatus_t BackendInit(Backend_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen, int mode);
/// @brief Delete frontend context
BackendStatus_t BackendDelete(Backend_t *context);

BackendStatus_t BackendRun(Backend_t *context);

BackendStatus_t translateToAsm(Backend_t *context);

#define RET_ON_ERROR(expr)                                          \
    do {                                                            \
        BackendStatus_t status = (expr);                            \
        if (status != BACKEND_SUCCESS) return status;               \
    } while (0)

#define SyntaxError(context, ret, ...)                              \
    do {                                                            \
        logPrint(L_ZERO, 1, "Error while translating to asm\n");    \
        logPrint(L_ZERO, 1, __VA_ARGS__);                           \
        return ret;                                                 \
    } while (0)
#endif
