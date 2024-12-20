#ifndef BACKEND_H
#define BACKEND_H

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
    BACKEND_IR_ERROR,
    BACKEND_FILE_ERROR,
    BACKEND_WRITE_ERROR,
    BACKEND_TYPE_ERROR,
    BACKEND_SCOPE_ERROR,
} BackendStatus_t;

typedef struct BackendContext_t {
    const char *inputFileName;
    const char *outputFileName;
    const char *text;

    NameTable_t nameTable;

    MemoryArena_t treeMemory;
    Node_t *tree;

    int mode;

    LocalsStack_t stk;
    bool inFunction;
    int operatorCounter;
    int ifCounter;
    int whileCounter;
} Backend_t;


typedef enum BackendMode_t {
    BACKEND_SIMPLE = 0,
    BACKEND_TAXES = 1
} BackendMode_t;

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
