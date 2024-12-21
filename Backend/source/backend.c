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

static bool cmpOp(Node_t *node, enum OperatorType op) {
    return node && node->type == OPERATOR && node->value.op == op;
}

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

/*========================Stack with local and global variables============================*/

static LocalVar_t *LocalsStackTop(LocalsStack_t *stk) {
    return (stk->size > 0) ? stk->vars + stk->size - 1 : NULL;
}

BackendStatus_t LocalsStackInit(LocalsStack_t *stk, size_t capacity) {
    stk->capacity = capacity;
    stk->vars = CALLOC(capacity, LocalVar_t);
    stk->size = 0;
    return BACKEND_SUCCESS;
}


BackendStatus_t LocalsStackDelete(LocalsStack_t *stk) {
    free(stk->vars);
    return BACKEND_SUCCESS;
}

BackendStatus_t LocalsStackPush(LocalsStack_t *stk, int id) {
    size_t addr = 0;
    if (stk->size > 0 && LocalsStackTop(stk)->id != FUNC_SCOPE)
        addr = LocalsStackTop(stk)->address + 1;

    stk->vars[stk->size].id = id;
    stk->vars[stk->size].address = addr;

    logPrint(L_EXTRA, 0, "Pushed variable %d to stk, addr = %d\n", id, addr);
    stk->size++;
    return BACKEND_SUCCESS;
}

//searches variable in stack and prints it
BackendStatus_t LocalsStackSearchPrint(LocalsStack_t *stk, int id, Backend_t *backend, FILE *file) {
    int idx = stk->size - 1;
    bool local = backend->inFunction;
    Identifier_t tableId = getIdFromTable(&backend->nameTable, id);
    logPrint(L_EXTRA, 0, "Searching for %d '%s'\n", id, tableId.str);

    if (tableId.type == FUNC_ID)
        SyntaxError(backend, BACKEND_TYPE_ERROR, "Identifier %s is function\n", tableId.str);

    while(idx >= 0) {
        LocalVar_t currentVar = stk->vars[idx];

        if (currentVar.id == id) {
            logPrint(L_EXTRA, 0, "Translating id %d %s\n", id, tableId.str);

            if (local)
                fprintf(file, "[rbx + %d] ; local %s\n", currentVar.address, tableId.str);
            else {
                fprintf(file, "[%d] ; global %s\n", currentVar.address, tableId.str);
            }
            return BACKEND_SUCCESS;
        } else if (currentVar.id == FUNC_SCOPE)
            local = false;

        idx--;
    }

    SyntaxError(backend, BACKEND_SCOPE_ERROR, "Variable %s wasn't declared at this scope\n", tableId.str);
}

BackendStatus_t LocalsStackPopScope(LocalsStack_t *stk) {
    logPrint(L_EXTRA, 0, "Popping scope\n");

    while (stk->size && LocalsStackTop(stk)->id >= 0)
        stk->size--;

    //stopped on scope separator
    if (stk->size) stk->size--;

    return BACKEND_SUCCESS;
}

BackendStatus_t LocalsStackInitScope(LocalsStack_t *stk, enum ScopeType scope) {
    logPrint(L_EXTRA, 0, "Creating new scope %d\n", scope);
    stk->vars[stk->size].id = scope;
    //if it is normal scope and we have elements before, address numeration continues
    if (scope == NORMAL_SCOPE && stk->size > 0) {
        stk->vars[stk->size].address = stk->vars[stk->size-1].address;
    }

    stk->size++;

    return BACKEND_SUCCESS;
}

/*==========================Backend===========================================================*/

BackendStatus_t BackendInit(Backend_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen, int mode) {

    context->inputFileName = inputFileName;
    context->outputFileName = outputFileName;

    NameTableCtor(&context->nameTable, maxTotalNamesLen, maxNametableSize);
    context->treeMemory = createMemoryArena(maxTokens, sizeof(Node_t));

    context->text = readFileToStr(inputFileName);

    LocalsStackInit(&context->stk, LOCALS_STACK_SIZE);
    context->operatorCounter = 1;
    context->ifCounter = 1;
    context->whileCounter = 1;

    if (mode)
        context->mode = BACKEND_TAXES;

    logPrint(L_EXTRA, 0, "Initialized backend\n");
    return BACKEND_SUCCESS;
}

BackendStatus_t BackendDelete(Backend_t *context) {
    assert(context);

    free( (void *) context->text);
    freeMemoryArena(&context->treeMemory);

    NameTableDtor(&context->nameTable);
    LocalsStackDelete(&context->stk);
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

static BackendStatus_t translateIf(Backend_t *context, FILE *file, Node_t *node) {
    assert(cmpOp(node, OP_IF));

    LocalsStackInitScope(&context->stk, NORMAL_SCOPE);

    int currentIf = context->ifCounter++;

    fprintf(file, "; if %d\n; conditional part\n", currentIf);
    RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));

    if (cmpOp(node->right, OP_ELSE)) {
        fprintf(file,   "PUSH 0\n"
                        "JE IF_ELSE%d\n", currentIf);
        fprintf(file, "; if %d statement part\n", currentIf);
        RET_ON_ERROR(translateToAsmRecursive(context, file, node->right->left));
        fprintf(file, "JMP IF_END%d\n", currentIf);

        fprintf(file, "; if %d else part\n", currentIf);
        fprintf(file, "IF_ELSE%d:\n", currentIf);
        RET_ON_ERROR(translateToAsmRecursive(context, file, node->right->right));
    } else {
        fprintf(file,   "PUSH 0\n"
                        "JE IF_END%d\n", currentIf);

        fprintf(file, "; if %d statement part\n", currentIf);
        RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
    }
    fprintf(file,   "IF_END%d:\n", currentIf);

    LocalsStackPopScope(&context->stk);
    return BACKEND_SUCCESS;
}

static BackendStatus_t translateWhile(Backend_t *context, FILE *file, Node_t *node) {
    LocalsStackInitScope(&context->stk, NORMAL_SCOPE);
    int currentWhile = context->whileCounter++;

    fprintf(file, "; LOOP %d\n; conditional part\n", currentWhile);
    fprintf(file, "LOOP%d:\n\n", currentWhile);
    RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
    fprintf(file,   "PUSH 0\n"
                    "JE LOOP_END%d\n", currentWhile);

    fprintf(file, "\n; while %d statement part\n", currentWhile);
    RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
    fprintf(file,   "\tJMP LOOP%d\n", currentWhile);
    fprintf(file,   "LOOP_END%d:\n", currentWhile);

    LocalsStackPopScope(&context->stk);
    return BACKEND_SUCCESS;
}

static BackendStatus_t translateCallArguments(Backend_t *context, FILE *file, Node_t *node) {
    fprintf(file, "; Pushing function arguments\n");
    size_t argIdx = 0;
    while (node) {
        Node_t *nextNode = NULL, *argNode = NULL;
        if (cmpOp(node, OP_COMMA)) {
            nextNode = node->right;
            argNode = node->left;
        } else {
            nextNode = NULL;
            argNode = node;
        }

        RET_ON_ERROR(translateToAsmRecursive(context, file, argNode));
        size_t offset = argIdx;
        if (context->stk.size > 0)
            offset += context->stk.vars[context->stk.size-1].address + 1;

        if (context->inFunction) {
            //popping arguments after the end of current frame
            fprintf(file, "POP [rbx + %d] \n", offset);
        } else
            fprintf(file, "POP [%d] ; global scope\n", offset);
        argIdx++;
        node = nextNode;
    }

    return BACKEND_SUCCESS;
}

static BackendStatus_t translateText(Backend_t *context, FILE *file, Node_t *node) {
    assert(context);
    assert(file);
    assert(cmpOp(node, OP_TEXT));
    const char *string = getIdFromTable(&context->nameTable, node->left->value.id).str;
//     //rodata is growing from end of RAM
//     static size_t memoryPointer = PROCESSOR_RAM_SIZE - 1;
//
//     fprintf(file, "; storing string %s\n", string);
//     while (string) {
//         fprintf(file, "PUSH %d ; '%c'\n"
//                       "POP  [%d]\n", *string, *string, memoryPointer--);
//         string++;
//     }
//     fprintf(file, "PUSH 0; end of string\n"
//                   "POP [%d]\n", memoryPointer--);
    fprintf(file, "; printing string %s\n", string);

    while (*string) {
        fprintf(file, "CHR %d ; %c\n", *string, *string);
        string++;
    }
    fprintf(file, "CHR %d ; end of string\n", '\n');
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
    //TODO: free stack on calls without assignment
    if (node->right)
        RET_ON_ERROR(translateCallArguments(context, file, node->right));

    size_t rbxShift = (context->stk.size > 0) ? LocalsStackTop(&context->stk)->address + 1: 0;
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

    LocalsStackInitScope(&context->stk, FUNC_SCOPE);
    context->inFunction = true;

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
        LocalsStackPush(&context->stk, argIdx);
        arg = arg->right;
    }

    RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));

    fprintf(file, "\nDECL_END_%s:\n", funcName);
    fprintf(file, "#----End of function %s------------#\n", funcName);

    context->inFunction = false;
    LocalsStackPopScope(&context->stk);
    return BACKEND_SUCCESS;
}

static BackendStatus_t translateToAsmRecursive(Backend_t *context, FILE *file, Node_t *node) {
    assert(context);
    assert(file);
    assert(node);

    if (node->type == NUMBER) {
        fprintf(file, "PUSH %lg\n", node->value.number);
        return BACKEND_SUCCESS;
    }

    if (node->type == IDENTIFIER) {
        fprintf(file, "PUSH ");
        RET_ON_ERROR(LocalsStackSearchPrint(&context->stk, node->value.id, context, file));
        return BACKEND_SUCCESS;
    }

    switch(node->value.op) {
        case OP_SEP:
            fprintf(file, "\n; %d\n", context->operatorCounter++);
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
            LocalsStackPush(&context->stk, node->left->value.id);
            break;
        }
        case OP_RET:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));

            fprintf(file, "POP rax ; Storing result value\n");
            if (context->mode == BACKEND_TAXES)
                fprintf(file, "CALL __TAXES ; taking taxes from returning value\n");
            fprintf(file, "RET\n");
            break;
        case OP_TEXT:
            RET_ON_ERROR(translateText(context, file, node));
            break;
        case OP_IF:
            RET_ON_ERROR(translateIf(context, file, node));
            break;
        case OP_WHILE:
            RET_ON_ERROR(translateWhile(context, file, node));
            break;
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
            RET_ON_ERROR(LocalsStackSearchPrint(&context->stk, node->left->value.id, context, file));
            break;
        case OP_OUT: case OP_SIN: case OP_COS: case OP_SQRT:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->left));
            fprintf(file, "%s\n", operators[node->value.op].asmStr);
            break;
        case OP_ASSIGN:
            RET_ON_ERROR(translateToAsmRecursive(context, file, node->right));
            fprintf(file, "POP ");
            RET_ON_ERROR(LocalsStackSearchPrint(&context->stk, node->left->value.id, context, file));
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
    fprintf(file,
    "__TAXES:\n"
    "   PUSH rax\n"
    "   PUSH %lf\n"
    "   MUL\n"
    "   POP rax\n"
    "   RET\n", TAX_COEFF);

    return BACKEND_SUCCESS;
}
