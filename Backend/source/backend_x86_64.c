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

#define asm_emit(...) fprintf(out, __VA_ARGS__);
#define TODO(msg) do {fprintf(stderr, "TODO: %s at %s:%d\n", msg, __FILE__, __LINE__); abort(); } while(0);


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

static BackendStatus_t includeStdlib(Backend_t *backend, FILE *out) {
    assert(backend); assert(out);

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
    int64_t bytesWrited = fwrite(buffer, 1, fileLen, out);
    assert(bytesWrited == fileLen);

    logPrint(L_ZERO, 0, "Writing stdlib to output file finished\n");

    free(buffer);
    fclose(stdlibFile);

    return BACKEND_SUCCESS;
}

static BackendStatus_t emitStart(Backend_t *backend, FILE *out) {
    assert(backend); assert(out);

    //TODO: change it
    asm_emit("global _start\n");
    asm_emit("_start:\n");

    asm_emit("; Saving frame pointer for global variables\n");
    asm_emit("\tmov  rbx, rsp\n");
    asm_emit("; Initializing xmm7 with 1.0\n");
    asm_emit("\tmov  rcx, 0x3FF0000000000000\n");
    asm_emit("\tmovq xmm7, rcx\n");


    return BACKEND_SUCCESS;
}

static BackendStatus_t translateIRarray(Backend_t *backend, FILE *out);

static BackendStatus_t translateBinaryMath(Backend_t *backend, FILE *out, IRNode_t *curNode);

BackendStatus_t translateIRtox86Asm(Backend_t *backend) {
    assert(backend);
    assert(backend->IR.nodes); assert(backend->IR.size > 0);

    FILE *out = fopen(backend->outputFileName, "w");
    if (!out) {
        logPrint(L_ZERO, 1, "Failed to open '%s' for writing\n", backend->outputFileName);
        return BACKEND_FILE_ERROR;
    }

    includeStdlib(backend, out);

    emitStart(backend, out);

    translateIRarray(backend, out);

    fclose(out);

    return BACKEND_SUCCESS;
}

static BackendStatus_t translateIRarray(Backend_t *backend, FILE *out) {
    assert(backend); assert(out);

    IR_t *IR = &backend->IR;
    IRNode_t *irNodes = IR->nodes;
    NameTable_t *nameTable = &backend->nameTable;

    for (size_t nodeIdx = 0; nodeIdx <  IR->size; nodeIdx++) {
        IRNode_t *curNode = IR->nodes + nodeIdx;

        bool printComment = (curNode->comment) && (curNode->type != IR_LABEL) && (curNode->type != IR_CALL);
        if (printComment)
            asm_emit("; %s\n", curNode->comment);

        switch(curNode->type) {
            case IR_NOP:
                break;

            case IR_LABEL:
                asm_emit("%s:\n", curNode->comment);
                break;

            case IR_ADD: case IR_SUB: case IR_MUL: case IR_DIV:
                RET_ON_ERROR(translateBinaryMath(backend, out, curNode));
                break;

            case IR_SQRT:
                asm_emit("\tmovq xmm0, [rsp]\n");
                asm_emit("\tsqrtsd xmm0, xmm0\n");
                asm_emit("\tmovq [rsp], xmm0\n");
                break;

            case IR_CMP:
                asm_emit("\tmovq xmm0, [rsp+8]\n");
                asm_emit("\t%s\n", IRcmpAsmStr[curNode->cmpType]);
                asm_emit("\tadd  rsp, 8\n");
                asm_emit("\tandpd xmm0, xmm7\n"); //xmm7 is 1.0
                asm_emit("\tmovq [rsp], xmm0\n");
                break;

            case IR_PUSH:
                switch(curNode->pushType) {
                    case PUSH_IMM:
                        asm_emit("\tmov  rcx, 0x%lX\n", *(uint64_t *)&curNode->dval);
                        asm_emit("\tpush rcx\n");
                        break;
                    case PUSH_REG: // push rax
                        asm_emit("\tpush rax\n");
                        break;
                    case PUSH_MEM:
                        if (curNode->local) {
                            asm_emit("\tpush QWORD [rbp + (%ji)]\n", curNode->addr.offset * 8);
                        } else {
                            asm_emit("\tpush QWORD [rbx + (%ji)]\n", curNode->addr.offset * 8);
                        }
                        break;
                    default:
                        assert(0);
                }
                break;

            case IR_POP:
                if (curNode->local) {
                    asm_emit("\tpop  QWORD [rbp + (%ji)]\n", curNode->addr.offset * 8);
                } else {
                    asm_emit("\tpop  QWORD [rbx + (%ji)]\n", curNode->addr.offset * 8);
                }

                break;

            case IR_VAR_DECL:
                asm_emit("\tsub  rsp, 8\n");
                break;

            case IR_JMP:
                asm_emit("\tjmp  %s\n", irNodes[curNode->addr.offset].comment);
                break;

            case IR_JZ:
                asm_emit("\tpop  rdi\n");
                asm_emit("\ttest rdi, rdi\n");
                asm_emit("\tjz   %s\n", irNodes[curNode->addr.offset].comment);
                break;

            case IR_CALL: {
                Identifier_t funcId = nameTable->identifiers[curNode->addr.offset];
                asm_emit("\tcall %s\n", funcId.str);
                if (funcId.argsCount > 0)
                    asm_emit("\tadd  rsp, %zu\n", funcId.argsCount * 8); // fixing stack

            }
                break;

            case IR_SET_FRAME_PTR:
                asm_emit("\tpush rbp\n");
                asm_emit("\tmov  rbp, rsp\n"); // first argument
                break;

            case IR_RET:
                // popping result to the rax from stack
                asm_emit("\tpop  rax\n");
                // fixing stack
                asm_emit("\tmov  rsp, rbp\n");
                // restoring rbp
                asm_emit("\tpop  rbp\n");
                asm_emit("\tret\n");
                break;

            case IR_EXIT:
                asm_emit("\tmov  rax, 0x3c\n");
                asm_emit("\tmov  rdi, 0\n");
                asm_emit("\tsyscall\n"); // exit syscall
                break;

            default:
                logPrint(L_ZERO, 1, "IR->x86: Unsopported node type %s\n", IRNodeTypeStrings[curNode->type]);
                return BACKEND_UNSUPPORTED_IR;
        }

    }

    return BACKEND_SUCCESS;
}

static BackendStatus_t translateBinaryMath(Backend_t *backend, FILE *out, IRNode_t *curNode) {
    assert(backend); assert(out); assert(curNode);

    asm_emit("\tmovq xmm0, [rsp+8]\n");

    switch(curNode->type) {
        case IR_ADD: asm_emit("\taddsd xmm0, [rsp]\n"); break;
        case IR_SUB: asm_emit("\tsubsd xmm0, [rsp]\n"); break;
        case IR_MUL: asm_emit("\tmulsd xmm0, [rsp]\n"); break;
        case IR_DIV: asm_emit("\tdivsd xmm0, [rsp]\n"); break;
        default: assert(0);
    }

    asm_emit("\tadd  rsp, 8\n");
    asm_emit("\tmovq [rsp], xmm0\n");

    return BACKEND_SUCCESS;
}
