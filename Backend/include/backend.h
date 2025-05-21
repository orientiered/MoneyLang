#ifndef BACKEND_H
#define BACKEND_H

#include <stdint.h>
#include <stdlib.h>

#include "backendStructs.h"

const double TAX_COEFF = 0.8;

const size_t BACKEND_MAX_FILENAME_LEN   = 64;

const char * const ASM_NAME_SUFFIX      = ".asm";
const char * const SPU_NAME_SUFFIX      = ".asm2";
const char * const LST_NAME_SUFFIX      = ".lst";
const char * const BIN_NAME_SUFFIX      = ".elf";

const char * const STDLIB_IN_FUNC_NAME  = "__stdlib_in";
const char * const STDLIB_OUT_FUNC_NAME = "__stdlib_out";

const char * const STDLIB_ASM_FILE      = "Backend/stdlib/stdlib.s";
const char * const STDLIB_BIN_FILE      = "Backend/stdlib/stdlib.elf";

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
    "IR_START",
    "IR_EXIT"
};

const size_t IR_MAX_SIZE = 4096;
const size_t IR_MAX_COMMENTS_LEN = 16384;

IR_t IRCtor(size_t capacity);
void IRDtor(IR_t *ir);

/* ====================================================================== */

/// @brief Initialize frontend context
BackendStatus_t BackendInit(Backend_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen, BackendMode_t mode);
/// @brief Delete frontend context
BackendStatus_t BackendDelete(Backend_t *context);

BackendStatus_t BackendRun(Backend_t *context);

/* ====================== Locals stack =================================== */
/* Used to handle scopes */
/* These functions are the same for both backends */
LocalVar_t *LocalsStackTop(LocalsStack_t *stk);

BackendStatus_t LocalsStackInit(LocalsStack_t *stk, size_t capacity);
BackendStatus_t LocalsStackDelete(LocalsStack_t *stk);

BackendStatus_t LocalsStackPopScope(LocalsStack_t *stk);
BackendStatus_t LocalsStackInitScope(LocalsStack_t *stk, enum ScopeType scope);

const size_t LOCALS_STACK_SIZE = 128;


/* ==================== Compilation for x86_64 ========================== */

BackendStatus_t IRdump(BackendContext_t *backend);
BackendStatus_t convertASTtoIR(BackendContext_t *backend, Node_t *ast);

BackendStatus_t translateIRtox86Asm(Backend_t *backend);


/* ==================== Compilation for SPU ============================= */
const size_t PROCESSOR_RAM_SIZE = 16384;

BackendStatus_t convertASTtoSPUAsm(Backend_t *backend);


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
