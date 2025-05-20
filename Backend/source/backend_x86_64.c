#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include <elf.h>

#include "utils.h"
#include "logger.h"
#include "nameTable.h"
#include "Context.h"

#include "backend.h"
#include "emitters_x86_64.h"

#define asm_emit(...) fprintf(backend->emitter.asmFirstPass, __VA_ARGS__);
#define TODO(msg) do {fprintf(stderr, "TODO: %s at %s:%d\n", msg, __FILE__, __LINE__); abort(); } while(0);

#define EMIT(emitterFunc, ...) \
    blockSize += emitterFunc(&backend->emitter,##__VA_ARGS__);


static const char * const IRcmpAsmStr[] = {
    "cmpltsd  xmm0, [rsp]", //CMP_LT,
    "cmpnlesd xmm0, [rsp]", //CMP_GT,
    "cmplesd  xmm0, [rsp]", //CMP_LE,
    "cmpnltsd xmm0, [rsp]", //CMP_GE,
    "cmpeqsd  xmm0, [rsp]", //CMP_EQ,
    "cmpneqsd xmm0, [rsp]"  //CMP_NEQ
};

static BackendStatus_t includeStdlib(Backend_t *backend, FILE *out);
static BackendStatus_t emitStart(Backend_t *backend, FILE *out);

static BackendStatus_t includeStdlib(Backend_t *backend) {
    assert(backend);

    logPrint(L_ZERO, 0, "Including stdlib\n");

    // Opening stdlib file
    FILE *stdlibFile = fopen(STDLIB_ASM_FILE, "r");
    if (!stdlibFile) {
        logPrint(L_ZERO, 1, "Failed to open stdlib file '%s'\n", STDLIB_ASM_FILE);
        return BACKEND_FILE_ERROR;
    }

    // Getting length of the file
    fseek(stdlibFile, 0, SEEK_END);
    int64_t fileLen = ftell(stdlibFile);
    assert(fileLen > 0);
    fseek(stdlibFile, 0, SEEK_SET);

    logPrint(L_ZERO, 0, "Stdlib length in bytes: %ji\n", fileLen);


    // Reading it to the buffer
    char *buffer = (char *) calloc(fileLen, 1);
    int64_t bytesRead = fread(buffer, 1, fileLen, stdlibFile);
    assert(bytesRead == fileLen);

    // Writing stdlib to the resulting file
    int64_t bytesWrited = fwrite(buffer, 1, fileLen, backend->emitter.asmFirstPass);
    assert(bytesWrited == fileLen);

    logPrint(L_ZERO, 0, "Writing stdlib to output file finished\n");

    free(buffer);
    fclose(stdlibFile);

    return BACKEND_SUCCESS;
}

static int32_t emitStart(Backend_t *backend) {
    assert(backend);
    int32_t blockSize = 0;
    //TODO: change it
    asm_emit("global _start\n");
    asm_emit("_start:\n");

    asm_emit("; Saving frame pointer for global variables\n");
    asm_emit("\tmov  rbx, rsp\n");
    EMIT(emitMovRegReg64, R_RBX, R_RSP);

    asm_emit("; Initializing xmm7 with 1.0\n");
    asm_emit("\tmov  rcx, 0x3FF0000000000000\n");
    EMIT(emitMovRegImm64, R_RCX, 0x3FF0000000000000);

    asm_emit("\tmovq xmm7, rcx\n");
    EMIT(emitMovqXmmReg64, R_XMM7, R_RCX);

    return blockSize;
}

static BackendStatus_t translateIRarray(Backend_t *backend);

static int32_t translateBinaryMath(Backend_t *backend, IRNode_t *curNode);

static BackendStatus_t emitCtxCtor(Backend_t *backend);
static BackendStatus_t emitCtxDtor(Backend_t *backend);

static BackendStatus_t emitCtxCtor(Backend_t *backend) {
    FILE *asmFirstPass = fopen(backend->outputFileName, "w");
    if (!asmFirstPass) {
        logPrint(L_ZERO, 1, "Failed to open '%s' for writing\n", backend->outputFileName);
        return BACKEND_FILE_ERROR;
    }

    const char * const emitAsmName = "__emitdbg.asm";
    const char * const binFileName = "__result.elf";
    FILE *asmFile = fopen(emitAsmName, "w");
    FILE *binFile = fopen(binFileName, "wb");

    if (!asmFile) {
        logPrint(L_ZERO, 1, "Failed to open '%s' for writing\n", emitAsmName);
        return BACKEND_FILE_ERROR;
    }

    if (!binFile) {
        logPrint(L_ZERO, 1, "Failed to open '%s' for writing\n", binFileName);
        return BACKEND_FILE_ERROR;
    }

    backend->emitter.asmFirstPass = asmFirstPass;
    backend->emitter.asmFile = asmFile;
    backend->emitter.binFile = binFile;
    //!!! It should be false
    backend->emitter.emitting = true;

    return BACKEND_SUCCESS;
}

static BackendStatus_t emitCtxDtor(Backend_t *backend) {
    fclose(backend->emitter.asmFirstPass);
    fclose(backend->emitter.asmFile);
    fclose(backend->emitter.binFile);

    return BACKEND_SUCCESS;
}


BackendStatus_t translateIRtox86Asm(Backend_t *backend) {
    assert(backend);
    assert(backend->IR.nodes); assert(backend->IR.size > 0);

    RET_ON_ERROR(emitCtxCtor(backend));

    includeStdlib(backend);

    emitStart(backend);

    translateIRarray(backend);

    emitCtxDtor(backend);

    return BACKEND_SUCCESS;
}

static int32_t translatePush(Backend_t *backend, IRNode_t *curNode);
static int32_t translatePop(Backend_t *backend, IRNode_t *curNode);

static BackendStatus_t translateIRarray(Backend_t *backend) {
    assert(backend);

    IR_t *IR = &backend->IR;
    IRNode_t *irNodes = IR->nodes;
    NameTable_t *nameTable = &backend->nameTable;


    int64_t startOffset = 0;

    for (size_t nodeIdx = 0; nodeIdx <  IR->size; nodeIdx++) {
        IRNode_t *curNode = IR->nodes + nodeIdx;

        bool printComment = (curNode->comment) && (curNode->type != IR_LABEL) && (curNode->type != IR_CALL);
        if (printComment)
            asm_emit("; %s\n", curNode->comment);

        int32_t blockSize = 0;

        switch(curNode->type) {
            case IR_NOP:
                break;

            case IR_LABEL:
                asm_emit("%s:\n", curNode->comment);
                break;

            case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
                blockSize = translateBinaryMath(backend, curNode);
                break;

            case IR_SQRT:
                asm_emit("\tmovq xmm0, [rsp]\n");
                EMIT(emitMovqXmmMemBaseDisp32, R_XMM0, R_RSP, 0);
                asm_emit("\tsqrtsd xmm0, xmm0\n");
                EMIT(emitSqrtsdXmm, R_XMM0);
                asm_emit("\tmovq [rsp], xmm0\n");
                EMIT(emitMovqMemBaseDisp32Xmm, R_RSP, 0, R_XMM0);
                break;

            case IR_CMP:
                asm_emit("\tmovq xmm0, [rsp+8]\n");
                EMIT(emitMovqXmmMemBaseDisp32, R_XMM0, R_RSP, 8);
                asm_emit("\t%s\n", IRcmpAsmStr[curNode->cmpType]);
                EMIT(emitCmpsdXmmMemBase, R_XMM0, R_RSP, curNode->cmpType);
                asm_emit("\tadd  rsp, 8\n");
                EMIT(emitAddReg64Imm32, R_RSP, 8);
                asm_emit("\tandpd xmm0, xmm7\n"); //xmm7 is 1.0
                EMIT(emitAndpd, R_XMM0, R_XMM7);
                asm_emit("\tmovq [rsp], xmm0\n");
                EMIT(emitMovqMemBaseDisp32Xmm, R_RSP, 0, R_XMM0);
                break;

            case IR_PUSH:
                blockSize += translatePush(backend, curNode);
                break;

            case IR_POP:
                blockSize += translatePop(backend, curNode);
                break;

            case IR_VAR_DECL:
                asm_emit("\tsub  rsp, 8\n");
                EMIT(emitSubReg64Imm32, R_RSP, 8);
                break;

            case IR_JMP:
                asm_emit("\tjmp  %s\n", irNodes[curNode->addr.offset].comment);
                //TODO: this is not correct address
                EMIT(emitJmp, curNode->addr.offset);
                break;

            case IR_JZ:
                asm_emit("\tpop  rdi\n");
                EMIT(emitPopReg64, R_RDI);
                asm_emit("\ttest rdi, rdi\n");
                EMIT(emitTest, R_RDI, R_RDI);
                asm_emit("\tjz   %s\n", irNodes[curNode->addr.offset].comment);
                //TODO: this is not correct address
                EMIT(emitJz, curNode->addr.offset);
                break;

            case IR_CALL: {
                Identifier_t funcId = nameTable->identifiers[curNode->addr.offset];
                asm_emit("\tcall %s\n", funcId.str);
                //TODO: this is not correct address
                EMIT(emitCall, curNode->addr.offset);
                if (funcId.argsCount > 0) {
                    asm_emit("\tadd  rsp, %zu\n", funcId.argsCount * 8); // fixing stack
                    EMIT(emitAddReg64Imm32, R_RSP, funcId.argsCount * 8);
                }

            }
                break;

            case IR_SET_FRAME_PTR:
                asm_emit("\tpush rbp\n");
                EMIT(emitPopReg64, R_RBP);
                asm_emit("\tmov  rbp, rsp\n"); // first argument
                EMIT(emitMovRegReg64, R_RBP, R_RSP);
                break;

            case IR_RET:
                // popping result to the rax from stack
                asm_emit("\tpop  rax\n");
                EMIT(emitPopReg64, R_RAX);
                // fixing stack
                asm_emit("\tmov  rsp, rbp\n");
                EMIT(emitMovRegReg64, R_RSP, R_RBP);
                // restoring rbp
                asm_emit("\tpop  rbp\n");
                EMIT(emitPopReg64, R_RBP);
                asm_emit("\tret\n");
                EMIT(emitRet);
                break;

            case IR_EXIT:
                asm_emit("\tmov  rax, 0x3c\n");
                EMIT(emitMovRegImm64, R_RAX, 0x3c);
                asm_emit("\tmov  rdi, 0\n");
                EMIT(emitMovRegImm64, R_RDI, 0);
                asm_emit("\tsyscall\n"); // exit syscall
                EMIT(emitSyscall);
                break;

            default:
                logPrint(L_ZERO, 1, "IR->x86: Unsopported node type %s\n", IRNodeTypeStrings[curNode->type]);
                return BACKEND_UNSUPPORTED_IR;
        }

        curNode->blockSize = blockSize;
        curNode->startOffset = startOffset;
        startOffset += blockSize;
    }

    return BACKEND_SUCCESS;
}

static int32_t translateBinaryMath(Backend_t *backend, IRNode_t *curNode) {
    assert(backend); assert(curNode);
    int32_t blockSize = 0;

    asm_emit("\tmovq xmm0, [rsp+8]\n");
    EMIT(emitMovqXmmMemBaseDisp32, R_XMM0, R_RSP, 8);

    switch(curNode->type) {
        case IR_ADD:
            asm_emit("\taddsd xmm0, [rsp]\n");
            EMIT(emitAddsdXmmMemBase, R_XMM0, R_RSP);
            break;
        case IR_SUB:
            asm_emit("\tsubsd xmm0, [rsp]\n");
            EMIT(emitSubsdXmmMemBase, R_XMM0, R_RSP);
            break;
        case IR_MUL:
            asm_emit("\tmulsd xmm0, [rsp]\n");
            EMIT(emitMulsdXmmMemBase, R_XMM0, R_RSP);
            break;
        case IR_DIV:
            asm_emit("\tdivsd xmm0, [rsp]\n");
            EMIT(emitDivsdXmmMemBase, R_XMM0, R_RSP);
            break;
        default: assert(0);
    }

    asm_emit("\tadd  rsp, 8\n");
    EMIT(emitAddReg64Imm32, R_RSP, 8);
    asm_emit("\tmovq [rsp], xmm0\n");
    EMIT(emitMovqMemBaseDisp32Xmm, R_RSP, 0, R_XMM0);

    return blockSize;
}

static int32_t translatePush(Backend_t *backend, IRNode_t *curNode) {
    assert(backend); assert(curNode);

    int32_t blockSize = 0;

    switch(curNode->pushType) {
        case PUSH_IMM:
            asm_emit("\tmov  rcx, 0x%lX\n", *(uint64_t *)&curNode->dval);
            EMIT(emitMovRegImm64, R_RCX, *(uint64_t *)&curNode->dval);
            asm_emit("\tpush rcx\n");
            EMIT(emitPushReg64, R_RCX);
            break;
        case PUSH_REG: // push rax
            asm_emit("\tpush rax\n");
            EMIT(emitPushReg64, R_RAX);
            break;
        case PUSH_MEM:
            if (curNode->local) {
                asm_emit("\tpush QWORD [rbp + (%ji)]\n", curNode->addr.offset * 8);
                EMIT(emitPushMemBaseDisp32, R_RBP, curNode->addr.offset * 8);
            } else {
                asm_emit("\tpush QWORD [rbx + (%ji)]\n", curNode->addr.offset * 8);
                EMIT(emitPushMemBaseDisp32, R_RBX, curNode->addr.offset * 8);
            }
            break;
        default:
            assert(0);
    }

    return blockSize;
}

static int32_t translatePop(Backend_t *backend, IRNode_t *curNode) {
    assert(backend); assert(curNode);

    int32_t blockSize = 0;

    if (curNode->local) {
        asm_emit("\tpop  QWORD [rbp + (%ji)]\n", curNode->addr.offset * 8);
        EMIT(emitPopMemBaseDisp32, R_RBP, curNode->addr.offset * 8);
    } else {
        asm_emit("\tpop  QWORD [rbx + (%ji)]\n", curNode->addr.offset * 8);
        EMIT(emitPopMemBaseDisp32, R_RBX, curNode->addr.offset * 8);
    }

    return blockSize;
}
