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


static BackendStatus_t translateCallArguments(Backend_t *context, FILE *file, Node_t *node) {
    fprintf(file, "; Pushing function arguments\n");
    size_t argIdx = 0;
    while (node) {
        Node_t *nextNode = NULL, *argNode = NULL;
        if (node->type == OPERATOR && node->value.op == OP_COMMA) {
            nextNode = node->right;
            argNode = node->left;
        } else {
            nextNode = NULL;
            argNode = node;
        }

        RET_ON_ERROR(translateToAsmRecursive(context, file, argNode));
        if (context->inFunction)
            //popping arguments after the end of current frame
            fprintf(file, "POP [rbx + %d] \n", context->localVars + argIdx);
        else
            fprintf(file, "POP [%d] ; global scope\n", context->globalVars + argIdx);
        argIdx++;
        node = nextNode;
    }

    return BACKEND_SUCCESS;
}

static BackendStatus_t translateCall(Backend_t *context, FILE *file, Node_t *node) {
    assert(context);
    assert(file);
    assert(node);
    assert(node->value.op == OP_CALL);

    Identifier_t func = getIdFromTable(&context->nameTable, node->left->value.id);
    if (func.type != FUNC_ID)
        SyntaxError(context, BACKEND_TYPE_ERROR, "Try to use variable %s as a function\n", func.str);
    //TODO: arguments count check
    if (node->right)
        RET_ON_ERROR(translateCallArguments(context, file, node->right));

    size_t rbxShift = (context->inFunction) ? context->localVars : context->globalVars;
    fprintf(file,
                "PUSH rbx ; storing current base pointer \n"
                "PUSH rbx + %d ; \n"
                "POP  rbx ; new rbx\n", rbxShift);

    fprintf(file, "CALL %s\n", func.str);

    fprintf(file, "POP  rbx ; restoring rbx\n");

    fprintf(file, "PUSH rax\n");
    return BACKEND_SUCCESS;
}
static BackendStatus_t translateFuncDecl(Backend_t *context, FILE *file, Node_t *node) {
    assert(context);
    assert(file);
    assert(node);
    assert(node->value.op == OP_FUNC_DECL);

    context->inFunction = true;
    context->localVars = 0;

    const char *funcName = getIdFromTable(&context->nameTable, node->left->left->value.id).str;
    fprintf(file, "#----Translating function %s-------#\n", funcName);
    fprintf(file, "JMP DECL_END_%s\n", funcName);
    fprintf(file, "%s:\n", funcName);

    //counting arguments
    Node_t *arg = node->left->right;
    while (arg) {
        size_t argIdx = (arg->type == OPERATOR) ? arg->left->value.id : arg->value.id;
        Identifier_t *id = context->nameTable.identifiers + argIdx;
        logPrint(L_EXTRA, 0, "Argument %p in function %s: %d %s\n", arg, funcName, argIdx, id->str);
        //writing relative address of argument variable
        id->address = context->localVars;
        id->local   = true;
        context->localVars++;
        arg = arg->right;
    }

    RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));

    fprintf(file, "\nDECL_END_%s:\n", funcName);
    fprintf(file, "#----End of function %s------------#\n", funcName);

    context->inFunction = false;
    return BACKEND_SUCCESS;
}

static BackendStatus_t translateVariable(Backend_t *context, FILE *file, Node_t *node) {
    Identifier_t id = getIdFromTable(&context->nameTable, node->value.id);
    if (id.type == UNDEFINED_ID)
        SyntaxError(context, BACKEND_TYPE_ERROR, "Identifier %s wasn't declared\n", id.str);
    if (id.type == FUNC_ID)
        SyntaxError(context, BACKEND_TYPE_ERROR, "Identifier %s is function name\n", id.str);

    logPrint(L_EXTRA, 0, "Translating id %d %s\n", node->value.id, id.str);
    if (!id.local) {
        logPrint(L_EXTRA, 0, "\tGlobal id\n");
        fprintf(file, "[%d]       ; %s\n", id.address, id.str);
    } else {
        logPrint(L_EXTRA, 0, "\tLocal id\n");
        if (!context->inFunction)
            SyntaxError(context, BACKEND_SCOPE_ERROR, "Try to use local variable in global scope\n");
        fprintf(file, "[rbx + %d] ; local %s\n", id.address, id.str);
    }
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
        fprintf(file, "PUSH ");
        RET_ON_ERROR(translateVariable(context, file, node));
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
            RET_ON_ERROR(translateCall(context, file, node));
            break;
        case OP_VAR_DECL:
        {
            Identifier_t *id = context->nameTable.identifiers + node->left->value.id;
            fprintf(file, "; Declaring variable %s\n", id->str);
            if (context->inFunction) {
                id->local = true;
                id->address = context->localVars;
                context->localVars++;
                fprintf(file, "; locals = %d\n", context->localVars);
            } else {
                id->local = false;
                id->address = context->globalVars;
                context->globalVars++;
                fprintf(file, "; globals = %d\n", context->globalVars);
            }
            break;
        }
        case OP_RET:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            fprintf(file, "POP rax ; Storing result value\n");
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
        case OP_GREAT_EQ:  case OP_LESS_EQ: case OP_EQUAL: case OP_NEQUAL:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
            fprintf(file, "%s\n", operators[node->value.op].asmStr);
            break;
        case OP_IN:
            fprintf(file, "IN\n");
            fprintf(file, "POP ");
            RET_ON_ERROR(translateVariable(context, file, node->left));
            break;
        case OP_OUT: case OP_SIN: case OP_COS: case OP_SQRT:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            fprintf(file, "%s\n", operators[node->value.op].asmStr);
            break;
        case OP_ASSIGN:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
            fprintf(file, "POP ");
            RET_ON_ERROR(translateVariable(context, file, node->left));
            break;
        default:
            logPrint(L_ZERO, 1, "Backend: Operator %d (%s) isn't supported yet\n", node->value.op, operators[node->value.op].dotStr);
            exit(1);
    }

    return BACKEND_SUCCESS;
}

BackendStatus_t translateAsmSTD(Backend_t *context, FILE *file);

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
    fprintf(file, "PUSH 0; zeroing rbx\n");
    fprintf(file, "POP rbx\n");

    RET_ON_ERROR(translateToAsmRecursive(context, file, context->tree));
    fprintf(file, "HLT\n\n");

    RET_ON_ERROR(translateAsmSTD(context, file));
    fclose(file);

    return BACKEND_SUCCESS;
}

BackendStatus_t translateAsmSTD(Backend_t *context, FILE *file) {
    const char *ineqCalls[] = {"__LESS", "__GREATER", "__GREATER_EQ", "__LESS_EQ", "__EQUAL", "__NEQUAL"};
    const char *ineqJumps[] = {"JB",     "JA",        "JAE",          "JBE",       "JE",      "JNE"};
    for (unsigned idx = 0; idx < ARRAY_SIZE(ineqCalls); idx++) {
        fprintf(file,
        "%s:\n"
        "\t%s %s1\n"
        "\tPUSH 0\n"
        "\tRET\n"
        "%s1:\n"
        "\tPUSH 1\n"
        "\tRET\n\n",
        ineqCalls[idx], ineqJumps[idx], ineqCalls[idx], ineqCalls[idx]);
    }
/*example
    __LESS:
        JB __LESS1
        PUSH 0
        RET
    __LESS1:
        PUSH 1
        RET
*/
    return BACKEND_SUCCESS;
}
