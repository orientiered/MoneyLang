#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include "string.h"

#include "utils.h"
#include "logger.h"
#include "nameTable.h"
#include "Context.h"
#include "backend.h"

static void backendToLangContext(LangContext_t *lContext, Backend_t *context) {
    lContext->inputFileName  = context->inputFileName;
    lContext->outputFileName = context->outputFileName;
    lContext->text           = context->text;
    lContext->nameTable      = context->nameTable;
    lContext->treeMemory     = context->treeMemory;
    lContext->tree           = context->tree;
    lContext->mode           = context->mode;
}

static void langContextToBackend(Backend_t *context, LangContext_t *lContext) {
    context->inputFileName   = lContext->inputFileName;
    context->outputFileName  = lContext->outputFileName;
    context->text            = lContext->text;
    context->nameTable       = lContext->nameTable;
    context->treeMemory      = lContext->treeMemory;
    context->tree            = lContext->tree;
    context->mode            = lContext->mode;
}

BackendStatus_t BackendInit(Backend_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen) {

    context->inputFileName = inputFileName;
    context->outputFileName = outputFileName;

    NameTableCtor(&context->nameTable, maxTotalNamesLen, maxNametableSize);
    context->treeMemory = createMemoryArena(maxTokens, sizeof(Node_t));

    context->text = readFileToStr(inputFileName);

    context->globalVars = 0;
    context->localVars = 0;

    logPrint(L_EXTRA, 0, "Initialized backend\n");
    return BACKEND_SUCCESS;
}

BackendStatus_t BackendDelete(Backend_t *context) {
    assert(context);

    free( (void *) context->text);
    freeMemoryArena(&context->treeMemory);

    NameTableDtor(&context->nameTable);
    memset(context, 0, sizeof(*context));
    logPrint(L_EXTRA, 0, "Deleted backend\n");
    return BACKEND_SUCCESS;
}

BackendStatus_t BackendRun(Backend_t *context) {
    LangContext_t lContext = {0};
    backendToLangContext(&lContext, context);
    IRStatus_t irStatus = readFromIR(&lContext);
    if (irStatus != IR_SUCCESS)
        return BACKEND_IR_ERROR;

    langContextToBackend(context, &lContext);
    DUMP_TREE(&lContext, context->tree, 0);

    BackendStatus_t status = translateToAsm(context);
    if (status != BACKEND_SUCCESS)
        return status;

    return BACKEND_SUCCESS;
}

static BackendStatus_t translateToAsmRecursive(Backend_t *context, FILE *file, Node_t *node);


static BackendStatus_t translateArguments(Backend_t *context, FILE *file, Node_t *node) {
    return BACKEND_SUCCESS;
}

static BackendStatus_t translateFuncDecl(Backend_t *context, FILE *file, Node_t *node) {
    assert(context);
    assert(file);
    assert(node);
    assert(node->value.op == OP_FUNC_DECL);

    const char *funcName = getIdFromTable(&context->nameTable, node->left->left->value.id).str;
    fprintf(file, "#----Translating function %s-------#\n", funcName);
    fprintf(file, "JMP DECL_END_%s\n", funcName);
    fprintf(file, "%s:\n", funcName);
    RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));

    fprintf(file, "\nDECL_END_%s:\n", funcName);
    fprintf(file, "#----End of function %s------------#\n", funcName);
    return BACKEND_SUCCESS;
}

static BackendStatus_t translateToAsmRecursive(Backend_t *context, FILE *file, Node_t *node) {
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
                getIdFromTable(&context->nameTable, node->value.id).str);
        return BACKEND_SUCCESS;
    }

    switch(node->value.op) {
        case OP_SEP:
            fprintf(file, "\n; %d\n", operatorCounter++);
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            if (node->right)
                RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));

            break;
        case OP_FUNC_DECL:
            RET_ON_ERROR(translateFuncDecl(context, file, node));
            break;
        case OP_CALL:
        {
            Identifier_t func = getIdFromTable(&context->nameTable, node->left->value.id);
            if (func.type != FUNC_ID)
                SyntaxError(context, BACKEND_TYPE_ERROR, "Try to use variable %s as a function\n", func.str);
            //TODO: arguments count check
            if (node->right)
                RET_ON_ERROR(translateArguments(context, file, node->right));
            fprintf(file, "CALL %s\n", func.str);
            fprintf(file, "PUSH RAX\n");
            break;
        }
        case OP_RET:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            fprintf(file, "POP RAX ; Storing result value\n");
            fprintf(file, "RET\n");
            break;
        case OP_IF:
        {
            int currentIf = ifCounter;
            ifCounter++;

            fprintf(file, "; if %d\n; conditional part\n", currentIf);
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            fprintf(file,   "PUSH 0\n"
                            "JE IF_END%d\n", currentIf);

            fprintf(file, "; if %d statement part\n", currentIf);
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
            fprintf(file,   "IF_END%d:\n", currentIf);
            break;
        }
        case OP_WHILE:
        {
            int currentWhile = whileCounter;
            whileCounter++;

            fprintf(file, "; LOOP %d\n; conditional part\n", currentWhile);
            fprintf(file, "LOOP%d:\n\n", currentWhile);
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            fprintf(file,   "PUSH 0\n"
                            "JE LOOP_END%d\n", currentWhile);

            fprintf(file, "\n; while %d statement part\n", currentWhile);
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
            fprintf(file,   "\tJMP LOOP%d\n", currentWhile);
            fprintf(file,   "LOOP_END%d:\n", currentWhile);
            break;
        }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        case OP_LABRACKET: case OP_RABRACKET:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
            fprintf(file, "%s\n", operators[node->value.op].asmStr);
            break;
        case OP_IN:
            fprintf(file, "IN\n");
            fprintf(file, "POP [%d] ; %s\n",  node->left->value.id,
                getIdFromTable(&context->nameTable, node->left->value.id).str);
            break;
        case OP_OUT: case OP_SIN: case OP_COS:
            translateToAsmRecursive(context, file, node->left);
            fprintf(file, "%s\n", operators[node->value.op].asmStr);
            break;
        case OP_ASSIGN:
            translateToAsmRecursive(context, file, node->right);
            fprintf(file, "POP [%d] ; %s\n",  node->left->value.id,
                getIdFromTable(&context->nameTable, node->left->value.id).str);
            break;
        default:
            logPrint(L_ZERO, 1, "Operator %d isn't supported yet\n", node->value.op);
            exit(1);
    }

    return BACKEND_SUCCESS;
}

BackendStatus_t translateToAsm(Backend_t *context) {
    assert(context);
    assert(context->tree);
    assert(context->outputFileName);

    FILE *file = fopen(context->outputFileName, "w");
    if (!file) {
        logPrint(L_ZERO, 1, "Can't open file '%s' for writing\n", context->outputFileName);
        return BACKEND_FILE_ERROR;
    }

    fprintf(file, "; Autogenerated\n");
    fprintf(file, "PUSH 0; zeroing RBX\n");
    fprintf(file, "POP RBX\n");

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
