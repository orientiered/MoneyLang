#ifndef BACKEND_H
#define BACKEND_H

#include <stdint.h>
#include <stdlib.h>

#include "backendStructs.h"

const char * const STDLIB_IN_FUNC_NAME  = "__stdlib_in";
const char * const STDLIB_OUT_FUNC_NAME = "__stdlib_out";
const char * const STDLIB_ASM_FILE      = "Backend/source/stdlib.s";

static const char * const IRNodeTypeStrings[] = {
    "IR_NOP", ///< use for commentarie"
    // binary arithmetical operations
    "IR_ADD",
    "IR_SUB",
    "IR_MUL",
    "IR_DIV",
    // unary arithmetical operations
    "IR_SQRT",
    // comparison operations
    "IR_CMP",
    // assign
    "IR_PUSH",
    "IR_POP",
    "IR_VAR_DECL",
    // control flow
    "IR_LABEL",
    "IR_JMP",
    "IR_JZ",
    "IR_CALL",
    "IR_RET",
    "IR_SET_FRAME_PTR",
    // IR_LEAVE,
    "IR_EXIT"
};

const size_t IR_MAX_SIZE = 4096;
const size_t IR_MAX_COMMENTS_LEN = 16384;

IR_t IRCtor(size_t capacity);
void IRDtor(IR_t *ir);

/* ====================================================================== */

BackendStatus_t IRdump(BackendContext_t *backend);
BackendStatus_t convertASTtoIR(BackendContext_t *backend, Node_t *ast);

BackendStatus_t LocalsStackInit(LocalsStack_t *stk, size_t capacity);
BackendStatus_t LocalsStackDelete(LocalsStack_t *stk);


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

BackendStatus_t translateIRtox86Asm(Backend_t *backend);

#define RET_ON_ERROR(expr)                                          \
    do {                                                            \
        BackendStatus_t status = (expr);                            \
        if (status != BACKEND_SUCCESS) {                            \
            logPrint(L_ZERO, 1, "Backend error: %s:%d\n", __FILE__, __LINE__);\
            return status;                                          \
        }                                                           \
    } while (0)

#define SyntaxError(context, ret, ...)                              \
    do {                                                            \
        logPrint(L_ZERO, 1, "Error while translating to asm\n");    \
        logPrint(L_ZERO, 1, __VA_ARGS__);                           \
        return ret;                                                 \
    } while (0)
#endif
