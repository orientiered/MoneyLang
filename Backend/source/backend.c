#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "string.h"

#include "utils.h"
#include "logger.h"
#include "nameTable.h"
#include "Context.h"
#include "backend.h"

BackendStatus_t BackendInit(LangContext_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen) {

    context->inputFileName = inputFileName;
    context->outputFileName = outputFileName;

    NameTableCtor(&context->nameTable, maxTotalNamesLen, maxNametableSize);
        context->treeMemory = createMemoryArena(maxTokens, sizeof(Node_t));

    context->text = readFileToStr(inputFileName);

    logPrint(L_EXTRA, 0, "Initialized backend\n");
    return BACKEND_SUCCESS;
}

BackendStatus_t BackendDelete(LangContext_t *context) {
    assert(context);

    free( (void *) context->text);
    freeMemoryArena(&context->treeMemory);

    NameTableDtor(&context->nameTable);
    memset(context, 0, sizeof(*context));
    logPrint(L_EXTRA, 0, "Deleted backend\n");
    return BACKEND_SUCCESS;
}

BackendStatus_t BackendRun(LangContext_t *context) {
    IRStatus_t irStatus = readFromIR(context);
    if (irStatus != IR_SUCCESS)
        return BACKEND_IR_ERROR;

    DUMP_TREE(context, context->tree, 0);

    BackendStatus_t status = translateToAsm(context);
    if (status != BACKEND_SUCCESS)
        return status;

    return BACKEND_SUCCESS;
}

static BackendStatus_t translateToAsmRecursive(LangContext_t *context, FILE *file, Node_t *node) {
    assert(context);
    assert(file);
    assert(node);

    static int operatorCounter = 1;
    static int ifCounter       = 1;
    static int whileCounter    = 1;

    if (node->type == NUMBER) {
        fprintf(file, "PUSH %lg\n", node->value.number);
        return BACKEND_SUCCESS;
    }

    if (node->type == IDENTIFIER) {
        fprintf(file, "PUSH [%d] ; %s\n", node->value.id,
                getIdentifier(&context->nameTable, node->value.id).str);
        return BACKEND_SUCCESS;
    }

    switch(node->value.op) {
        case OP_SEMICOLON:
            fprintf(file, "\n; %d\n", operatorCounter++);
            translateToAsmRecursive(context, file, node->left);
            if (node->right)
                translateToAsmRecursive(context, file, node->right);

            break;
        case OP_IF:
        {
            int currentIf = ifCounter;
            ifCounter++;

            fprintf(file, "; if %d\n; conditional part\n", currentIf);
            translateToAsmRecursive(context, file, node->left);
            fprintf(file,   "PUSH 0\n"
                            "JE IF_END%d\n", currentIf);

            fprintf(file, "; if %d statement part\n", currentIf);
            translateToAsmRecursive(context, file, node->right);
            fprintf(file,   "IF_END%d:\n", currentIf);
            break;
        }
        case OP_WHILE:
        {
            int currentWhile = whileCounter;
            whileCounter++;

            fprintf(file, "; LOOP %d\n; conditional part\n", currentWhile);
            fprintf(file, "LOOP%d:\n\n", currentWhile);
            translateToAsmRecursive(context, file, node->left);
            fprintf(file,   "PUSH 0\n"
                            "JE LOOP_END%d\n", currentWhile);

            fprintf(file, "\n; while %d statement part\n", currentWhile);
            translateToAsmRecursive(context, file, node->right);
            fprintf(file,   "\tJMP LOOP%d\n", currentWhile);
            fprintf(file,   "LOOP_END%d:\n", currentWhile);
            break;
        }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        case OP_LABRACKET: case OP_RABRACKET:
            translateToAsmRecursive(context, file, node->left);
            translateToAsmRecursive(context, file, node->right);
            fprintf(file, "%s\n", operators[node->value.op].asmStr);
            break;
        case OP_IN:
            fprintf(file, "IN\n");
            fprintf(file, "POP [%d] ; %s\n",  node->left->value.id,
                getIdentifier(&context->nameTable, node->left->value.id).str);
            break;
        case OP_OUT: case OP_SIN: case OP_COS:
            translateToAsmRecursive(context, file, node->left);
            fprintf(file, "%s\n", operators[node->value.op].asmStr);
            break;
        case OP_ASSIGN:
            translateToAsmRecursive(context, file, node->right);
            fprintf(file, "POP [%d] ; %s\n",  node->left->value.id,
                getIdentifier(&context->nameTable, node->left->value.id).str);
            break;
        default:
            logPrint(L_ZERO, 1, "Operator %d isn't supported yet\n", node->value.op);
            exit(1);
    }

    return BACKEND_SUCCESS;
}

BackendStatus_t translateToAsm(LangContext_t *context) {
    assert(context);
    assert(context->tree);
    assert(context->outputFileName);

    FILE *file = fopen(context->outputFileName, "w");
    if (!file) {
        logPrint(L_ZERO, 1, "Can't open file '%s' for writing\n", context->outputFileName);
        return BACKEND_FILE_ERROR;
    }

    fprintf(file, "; Autogenerated\n");
    translateToAsmRecursive(context, file, context->tree);
    fprintf(file, "HLT\n\n");
    fprintf(file,
    "__LESS:\n"
    "\tJA __LESS0\n"
    "\tPUSH 1\n"
    "\tRET\n"
    "__LESS0:\n"
    "\tPUSH 0\n"
    "\tRET\n\n"
    );
    fprintf(file,
    "__GREATER:\n"
    "\tJB __GREATER0\n"
    "\tPUSH 1\n"
    "\tRET\n"
    "__GREATER0:\n"
    "\tPUSH 0\n"
    "\tRET\n"
    );
    fclose(file);

    return BACKEND_SUCCESS;
}
