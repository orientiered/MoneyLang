#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "assert.h"
#include "string.h"

#include "utils.h"
#include "logger.h"
#include "nameTable.h"
#include "Context.h"
#include "backend.h"

#define asm_emit(...) fprintf(out, __VA_ARGS__);
#define TODO(msg) do {fprintf(stderr, "TODO: %s at %s:%d\n", msg, __FILE__, __LINE__); abort(); } while(0);

static const char * const IRasmStr[] = {
    "", //IR_NOP, ///< use for commentaries
    // binary arithmetical operations
    "addsd xmm0, [rsp]", //IR_ADD,
    "subsd xmm0, [rsp]", //IR_SUB,
    "mulsd xmm0, [rsp]", //IR_MUL,
    "divsd xmm0, [rsp]", //IR_DIV,
    // unary arithmetical operations
    "sqrtsd xmm0, [rsp]",//IR_SQRT,
    // comparison operations
    "cmpltsd xmm0, [rsp]", //IR_CMPLT,
    "cmpgtsd xmm0, [rsp]", //IR_CMPGT,
    "cmplesd xmm0, [rsp]", //IR_CMPLE,
    "cmpgesd xmm0, [rsp]", //IR_CMPGE,
    "cmpeqsd xmm0, [rsp]", //IR_CMPEQ,
    "cmpneqsd xmm0, [rsp]", //IR_CMPNEQ,
    // assign
    "", //IR_ASSIGN, //? Maybe redundant
    "", //IR_PUSH,
    "", //IR_POP,
    "", //IR_VAR_DECL,
    // control flow
    "", //IR_LABEL,
    "", //IR_JMP,
    "", //IR_JZ,
    "", //IR_CALL,
    "", //IR_RET,
    "", //IR_SET_FRAME_PTR,
    // IR_LEAVE,
    "" //IR_EXIT

};

BackendStatus_t translateIRtox86Asm(Backend_t *backend) {
    assert(backend);
    assert(backend->IR.nodes); assert(backend->IR.size > 0);

    FILE *out = fopen(backend->outputFileName, "w");
    if (!out) {
        logPrint(L_ZERO, 1, "Failed to open '%s' for writing\n", backend->outputFileName);
        return BACKEND_FILE_ERROR;
    }

    IR_t *IR = &backend->IR;
    IRNode_t *irNodes = IR->nodes;
    NameTable_t *nameTable = &backend->nameTable;

    //TODO: change it
    asm_emit("; Saving frame pointer for global variables\n");
    asm_emit("\tmov  rbx, rsp\n");
    asm_emit("; Initializing xmm7 with 1.0\n");
    asm_emit("\tmov  rcx, 0x3FF0000000000000\n");
    asm_emit("\tmovq xmm7, rcx\n");

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
                asm_emit("\tmovq xmm0, [rsp+8]\n");
                asm_emit("\t%s\n", IRasmStr[curNode->type]);
                asm_emit("\tadd  rsp, 8\n");
                asm_emit("\tmovq [rsp], xmm0\n");
                break;

            case IR_SQRT:
                asm_emit("\tmovq xmm0, [rsp]\n");
                asm_emit("\tsqrtsd xmm0\n");
                asm_emit("\tmovq [rsp], xmm0\n");
                break;

            case IR_CMPLT: case IR_CMPGT: case IR_CMPLE:
            case IR_CMPGE: case IR_CMPEQ: case IR_CMPNEQ:
                asm_emit("\tmovq xmm0, [rsp+8]\n");
                asm_emit("\t%s\n", IRasmStr[curNode->type]);
                asm_emit("\tadd  rsp, 8\n");
                asm_emit("\tandpd xmm0, xmm7\n"); //xmm7 is 1.0
                asm_emit("\tmovq [rsp], xmm0\n");
                break;

            case IR_PUSH:
                switch(curNode->extra) {
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
                // moving result to the rax from stack
                asm_emit("\tmov  rax, QWORD [rsp]\n");
                asm_emit("\tadd  rsp, 8\n");
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


    fclose(out);

    return BACKEND_SUCCESS;
}
