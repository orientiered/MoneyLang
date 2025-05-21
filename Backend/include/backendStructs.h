#ifndef BACKEND_STRUCTS_H
#define BACKEND_STRUCTS_H

#include <stdio.h>
#include <stdint.h>

#include "utils.h"
#include "nameTable.h"
#include "Context.h"

typedef struct LocalVar_t {
    int id;
    int64_t address;
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
    BACKEND_NESTED_FUNC_ERROR,
    BACKEND_WRONG_ARGS_NUMBER,
    BACKEND_UNSUPPORTED_IR,
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
    IR_CMP,
    // assign
    IR_PUSH,
    IR_POP,
    IR_VAR_DECL,
    // control flow
    IR_LABEL,
    IR_JMP,
    IR_JZ,
    IR_CALL,
    IR_RET,
    IR_SET_FRAME_PTR,
    // IR_LEAVE,
    IR_START,
    IR_EXIT

} IRNodeType_t;

enum IRPushPopType {
    PUSH_IMM,
    PUSH_MEM,
    PUSH_REG, // push rax
    POP_MEM
};

enum IRCmpType {
    CMP_LT,
    CMP_GT,
    CMP_LE,
    CMP_GE,
    CMP_EQ,
    CMP_NEQ
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
    bool local;

    union {
        enum IRPushPopType pushType;
        enum IRCmpType     cmpType;
    };

    const char *comment;
    int32_t blockSize;
    int64_t startOffset;
} IRNode_t;

typedef struct {
    IRNode_t *nodes;
    uint32_t size;
    uint32_t capacity;

    char *comments;
    char *commentPtr;
} IR_t;


enum ScopeType {
    FUNC_SCOPE = -1,
    NORMAL_SCOPE = -2
};

/* ======================== Emitters ========================================*/
enum REGS {
    R_RAX = 0,
    R_RCX = 1,
    R_RDX = 2,
    R_RBX = 3,
    R_RSP = 4,
    R_RBP = 5,
    R_RSI = 6,
    R_RDI = 7,

    R_R8  = 0 + 8,
    R_R9  = 1 + 8,
    R_R10 = 2 + 8,
    R_R11 = 3 + 8,
    R_R12 = 4 + 8,
    R_R13 = 5 + 8,
    R_R14 = 6 + 8,
    R_R15 = 7 + 8
};

enum XMMS {
    R_XMM0 = 0,
    R_XMM1 = 1,
    R_XMM2 = 2,
    R_XMM3 = 3,
    R_XMM4 = 4,
    R_XMM5 = 5,
    R_XMM6 = 6,
    R_XMM7 = 7
};
typedef enum XMMS XMM_t;
typedef enum REGS REG_t;

typedef struct {
    FILE *binFile;
    FILE *asmFile;

    FILE *asmFirstPass;
    bool emitting;
} emitCtx_t;

/* =================== Backend context ============================ */
typedef struct BackendContext_t {
    const char *inputFileName;
    const char *outputFileName;
    const char *text;

    NameTable_t nameTable;

    MemoryArena_t treeMemory;
    Node_t *tree;
    IR_t IR;
    emitCtx_t emitter;

    int mode;

    LocalsStack_t stk;
    bool inFunction;
    int operatorCounter;
    int ifCounter;
    int whileCounter;
} Backend_t;

#endif
