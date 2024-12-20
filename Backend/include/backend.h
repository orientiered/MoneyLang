#ifndef BACKEND_H
#define BACKEND_H

#include "Context.h"

typedef struct BackendContext_t {
    const char *inputFileName;
    const char *outputFileName;
    const char *text;

    NameTable_t nameTable;

    MemoryArena_t treeMemory;
    Node_t *tree;

    int mode;
    bool inFunction;
    size_t globalVars;
    size_t localVars;
    int operatorCounter;
    int ifCounter;
    int whileCounter;
} Backend_t;

typedef enum BackendStatus_t {
    BACKEND_SUCCESS,
    BACKEND_IR_ERROR,
    BACKEND_FILE_ERROR,
    BACKEND_WRITE_ERROR,
    BACKEND_TYPE_ERROR,
    BACKEND_SCOPE_ERROR,
} BackendStatus_t;

typedef enum BackendMode_t {
    BACKEND_SIMPLE = 0,
    BACKEND_TAXES = 1
} BackendMode_t;

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
