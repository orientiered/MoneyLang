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
/*
    =
x       y + 5

PUSH 5
PUSH [2]; y
ADD

POP [1]; x

*/
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
            else
                fprintf(file, "HLT\n");

            break;
        case OP_ADD:
            translateToAsmRecursive(context, file, node->left);
            translateToAsmRecursive(context, file, node->right);
            fprintf(file, "ADD\n");
            break;
        case OP_SUB:
            translateToAsmRecursive(context, file, node->left);
            translateToAsmRecursive(context, file, node->right);
            fprintf(file, "SUB\n");
            break;
        case OP_MUL:
            translateToAsmRecursive(context, file, node->left);
            translateToAsmRecursive(context, file, node->right);
            fprintf(file, "MUL\n");
            break;
        case OP_DIV:
            translateToAsmRecursive(context, file, node->left);
            translateToAsmRecursive(context, file, node->right);
            fprintf(file, "DIV\n");
            break;
        case OP_IN:
            fprintf(file, "IN\n");
            fprintf(file, "POP [%d] ; %s\n",  node->left->value.id,
                getIdentifier(&context->nameTable, node->left->value.id).str);
            break;
        case OP_OUT:
            translateToAsmRecursive(context, file, node->left);
            fprintf(file, "OUT\n");
            break;
        case OP_SIN:
            translateToAsmRecursive(context, file, node->left);
            fprintf(file, "SIN\n");
            break;
        case OP_COS:
            translateToAsmRecursive(context, file, node->left);
            fprintf(file, "COS\n");
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
    fclose(file);

    return BACKEND_SUCCESS;
}
