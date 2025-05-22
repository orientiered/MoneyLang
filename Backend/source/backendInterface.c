#include <stdio.h>
#include <assert.h>
#include <string.h>

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
}

static void langContextToBackend(Backend_t *context, LangContext_t *lContext) {
    context->inputFileName   = lContext->inputFileName;
    context->outputFileName  = lContext->outputFileName;
    context->text            = lContext->text;
    context->nameTable       = lContext->nameTable;
    context->treeMemory      = lContext->treeMemory;
    context->tree            = lContext->tree;
}

BackendStatus_t BackendInit(Backend_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen, BackendMode_t mode) {

    context->inputFileName = inputFileName;
    context->outputFileName = outputFileName;

    NameTableCtor(&context->nameTable, maxTotalNamesLen, maxNametableSize);
    context->treeMemory = createMemoryArena(maxTokens, sizeof(Node_t));

    context->text = readFileToStr(inputFileName);
    if (!context->text)
        return BACKEND_FILE_ERROR;

    LocalsStackInit(&context->stk, LOCALS_STACK_SIZE);
    context->operatorCounter = 1;
    context->ifCounter = 1;
    context->whileCounter = 1;
    context->mode = mode;

    logPrint(L_EXTRA, 0, "Initialized backend\n");
    return BACKEND_SUCCESS;
}

BackendStatus_t BackendDelete(Backend_t *context) {
    assert(context);

    free( (void *) context->text);
    freeMemoryArena(&context->treeMemory);

    NameTableDtor(&context->nameTable);
    IRDtor(&context->IR);
    LocalsStackDelete(&context->stk);
    memset(context, 0, sizeof(*context));
    logPrint(L_EXTRA, 0, "Deleted backend\n");
    return BACKEND_SUCCESS;
}

static void initStdlibFunctions(Backend_t *backend) {
    NameTable_t *nameTable = &backend->nameTable;
    insertIdentifier(nameTable, STDLIB_IN_FUNC_NAME);

    int outId = insertIdentifier(nameTable, STDLIB_OUT_FUNC_NAME);
    // stdlib out takes 1 argument for input
    nameTable->identifiers[outId].argsCount = 1;
}

BackendStatus_t BackendRun(Backend_t *context) {
    LangContext_t lContext = {0};
    backendToLangContext(&lContext, context);
    ASTStatus_t astStatus = readFromAST(&lContext);
    if (astStatus != AST_SUCCESS)
        return BACKEND_AST_ERROR;

    langContextToBackend(context, &lContext);
    DUMP_TREE(&lContext, context->tree, 0);


    BackendStatus_t status = BACKEND_SUCCESS;

    if (context->mode.spu) {
        status = convertASTtoSPUAsm(context);
        if (status != BACKEND_SUCCESS) {
            logPrint(L_ZERO, 1, "Failed to convert AST to SPU asm\n");
        }
        return status;
    }

    initStdlibFunctions(context);

    logPrint(L_ZERO, 0, "Converting AST to IR\n");
    status = convertASTtoIR(context, context->tree);
    if (status != BACKEND_SUCCESS) {
        logPrint(L_ZERO, 1, "Failed to convert AST to IR\n");
        return status;
    }

    IRdump(context);

    status = translateIRtox86Asm(context);
    if (status != BACKEND_SUCCESS) {
        logPrint(L_ZERO, 1, "Failed to convert IR to x86asm\n");
        return status;
    }

    return BACKEND_SUCCESS;
}
