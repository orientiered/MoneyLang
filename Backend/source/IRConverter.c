#include <stdio.h>
#include <logger.h>
#include <assert.h>
#include <stdarg.h>

#include "backend.h"

#define CALLOC(elemNumber, Type) (Type *) calloc(elemNumber, sizeof(Type))
#define TODO(msg) do {fprintf(stderr, "TODO: %s at %s:%d\n", msg, __FILE__, __LINE__); abort(); } while(0);

/* ================ Utils for AST ============================ */
static bool cmpOp(Node_t *node, enum OperatorType op) {
    return node && node->type == OPERATOR && node->value.op == op;
}

/* ================ IR functions      ================= */

static IRNode_t *IRgetNewNode(BackendContext_t *backend) {
    IR_t *IR = &backend->IR;
    assert(IR->size < IR->capacity);

    return &IR->nodes[IR->size++];
}

static IRNode_t *IRnodeCtor(BackendContext_t *backend, IRNodeType_t type);

static IRNode_t *IRnodeCtor(BackendContext_t *backend, IRNodeType_t type) {
    IRNode_t *node = IRgetNewNode(backend);
    node->type = type;

    return node;
}


IR_t IRCtor(size_t capacity) {
    IR_t ir = {
        .nodes = CALLOC(capacity, IRNode_t),
        .size = 0,
        .capacity = capacity,
        .comments = CALLOC(IR_MAX_COMMENTS_LEN, char),
    };

    assert(ir.nodes);
    assert(ir.comments);

    ir.commentPtr = ir.comments;

    return ir;
}

void IRDtor(IR_t *ir) {
    free(ir->nodes); ir->nodes = NULL;
    free(ir->comments); ir->comments = NULL;
    ir->commentPtr = NULL;

    ir->capacity = 0;
}

/// @brief Print comment to the current node in IR
void IRprintf(BackendContext_t *backend, const char *fmt, ...) __attribute__ ( (format (printf, 2, 3)) );

void IRprintf(BackendContext_t *backend, const char *fmt, ...) {
    IR_t *ir = &backend->IR;

    va_list args;
    va_start(args, fmt);

    ir->nodes[ir->size].comment = ir->commentPtr;
    ir->commentPtr += vsprintf(ir->commentPtr, fmt, args) + 1;

    va_end(args);
}

static uint32_t IRgetNodeIdx(BackendContext_t *backend, const IRNode_t *irNode) {
    return (uint32_t) (irNode - backend->IR.nodes) / sizeof(irNode);
}

/// @brief Create IR_LABEL node and return its index in IR array
/// By default label is local
/// Global labels are used for function declarations

static uint32_t IRcreateLabel(BackendContext_t *backend, const char *fmt, ...) __attribute__ ( (format (printf, 2, 3)) );

static uint32_t IRcreateLabel(BackendContext_t *backend, const char *fmt, ...) {
    IR_t *ir = &backend->IR;

    IRNode_t *label = IRgetNewNode(backend);
    label->type = IR_LABEL;
    uint32_t idx = ir->size - 1;

    va_list args;
    va_start(args, fmt);

    label->comment = ir->commentPtr;
    ir->commentPtr += vsprintf(ir->commentPtr, fmt, args) + 1;

    va_end(args);

    label->local = true;

    return idx;
}

void IRComment(BackendContext_t *backend, const char *fmt, ...) __attribute__ ( (format (printf, 2, 3)) );

void IRComment(BackendContext_t *backend, const char *fmt, ...) {
    IR_t *ir = &backend->IR;

    va_list args;
    va_start(args, fmt);

    assert(ir->size < IR_MAX_SIZE);
    IRNode_t *node = &ir->nodes[ir->size];

    node->type = IR_NOP;
    node->comment = ir->commentPtr;
    ir->commentPtr += vsprintf(ir->commentPtr, fmt, args) + 1;

    ir->size++;

    va_end(args);
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
    // addresses are relative to rbp (or rbx for global variables) without *8
    //  3 | arg 2
    //  2 | arg 1
    //  1 | return address
    //  0 | rbp
    // -1 | local 1
    // -2 | local 2
    // -3 | local 3

    int64_t addr = -1;
    if (stk->size > 0 && LocalsStackTop(stk)->id != FUNC_SCOPE) {
        int64_t topAddr = LocalsStackTop(stk)->address;
        if (topAddr < 0)
            addr = LocalsStackTop(stk)->address - 1;
    }

    stk->vars[stk->size].id = id;
    stk->vars[stk->size].address = addr;

    logPrint(L_EXTRA, 0, "Pushed variable %d to stk, addr = %ji\n", id, addr);
    stk->size++;
    return BACKEND_SUCCESS;
}

/// argNumber starts from 0, counting left to right
static BackendStatus_t LocalsStackPushFuncArg(LocalsStack_t *stk, int id, int argNumber) {
    stk->vars[stk->size].id = id;
    int64_t addr = argNumber + 2;
    stk->vars[stk->size].address = addr;

    stk->size++;

    logPrint(L_EXTRA, 0, "Pushed variable %d (function argument) to stk, addr = %ji\n", id, addr);

    return BACKEND_SUCCESS;
}

//searches variable in stack and prints it
static BackendStatus_t LocalsStackSearchAddr(int id, Backend_t *backend, IRNode_t *irNode);

static BackendStatus_t LocalsStackSearchAddr(int id, Backend_t *backend, IRNode_t *irNode) {
    assert(backend);
    LocalsStack_t *stk = &backend->stk;

    int idx = stk->size - 1;
    bool local = backend->inFunction;
    Identifier_t tableId = getIdFromTable(&backend->nameTable, id);
    logPrint(L_EXTRA, 0, "Searching for %d '%s'\n", id, tableId.str);

    if (tableId.type == FUNC_ID)
        SyntaxError(backend, BACKEND_TYPE_ERROR, "Identifier %s is function\n", tableId.str);

    while(idx >= 0) {
        LocalVar_t currentVar = stk->vars[idx];

        if (currentVar.id == id) {
            logPrint(L_EXTRA, 0, "Found id %d %s, local = %d\n", id, tableId.str, local);

            IR_t *ir = &backend->IR;
            irNode->local = local;
            irNode->comment = ir->commentPtr;
            if (local)
                ir->commentPtr += sprintf(ir->commentPtr, "local %s", tableId.str) + 1;
            else
                ir->commentPtr += sprintf(ir->commentPtr, "global %s", tableId.str) + 1;

            irNode->addr.offset = currentVar.address;

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
/* ======================================= AST to IR converter ======================================= */
static BackendStatus_t convertASTtoIRrecursive(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertBinaryArithmetic(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertUnaryArithmetic(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertComparison(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertSeparator(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertAssign(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertIfElse(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertWhile(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertVarDeclaration(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertFuncDecl(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertRet(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertCall(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertIn(BackendContext_t *backend, Node_t *node);

static BackendStatus_t convertOut(BackendContext_t *backend, Node_t *node);

BackendStatus_t convertASTtoIR(BackendContext_t *backend, Node_t *ast) {
    assert(backend);
    assert(ast);

    logPrint(L_ZERO, 0, "Constructing IR\n");
    IR_t ir = IRCtor(IR_MAX_SIZE);
    backend->IR = ir;

    // Adding start node
    IRprintf(backend, "--------- Program start ----------");
    IRnodeCtor(backend, IR_START);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, ast));

    // adding exit node
    IRprintf(backend, "--------- Program exit -------------");
    IRnodeCtor(backend, IR_EXIT);

    return BACKEND_SUCCESS;
}

BackendStatus_t IRdump(BackendContext_t *backend) {
    assert(backend);

    logPrint(L_ZERO, 0, "Dumpring IR to irDumpt.txt file\n");
    FILE *out = fopen("irDump.txt", "w");

    IR_t *ir = &backend->IR;

    for (size_t idx = 0; idx < ir->size; idx++) {
        IRNode_t *node = ir->nodes + idx;
        fprintf(out, "%zu:\n", idx);
        fprintf(out, "\t TYPE=%s, COMMENT=%s\n", IRNodeTypeStrings[node->type], node->comment);
        if (node->type == IR_JMP)
            fprintf(out, "\tjmp to node %ji\n", node->addr.offset);
        else if (node->type == IR_PUSH && node->pushType == PUSH_IMM)
            fprintf(out, "\tpush %lf", node->dval);


    }

    fclose(out);

    return BACKEND_SUCCESS;
}


static BackendStatus_t convertASTtoIRrecursive(BackendContext_t *backend, Node_t *node) {
    assert(backend);
    assert(node);

    if (node->type == NUMBER) {
        logPrint(L_ZERO, 0, "ASTtoIR: Converting number\n");
        IRNode_t *irNode = IRnodeCtor(backend, IR_PUSH);
        irNode->pushType = PUSH_IMM;

        irNode->dval = node->value.number;

        return BACKEND_SUCCESS;
    }

    if (node->type == IDENTIFIER) {
        logPrint(L_ZERO, 0, "ASTtoIR: Converting identifier\n");
        IRNode_t *irNode = IRnodeCtor(backend, IR_PUSH);
        irNode->pushType = PUSH_MEM;

        int nameId = node->value.id;

        RET_ON_ERROR(LocalsStackSearchAddr(nameId, backend, irNode));
        return BACKEND_SUCCESS;
    }


    switch(node->value.op) {
        case OP_SEP:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting separator\n");
            RET_ON_ERROR(convertSeparator(backend, node));
            break;

        case OP_VAR_DECL:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting variable declaration\n");
            RET_ON_ERROR(convertVarDeclaration(backend, node));
            break;

        case OP_FUNC_DECL:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting function declaration\n");
            RET_ON_ERROR(convertFuncDecl(backend, node));
            break;

        case OP_CALL:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting call \n");
            RET_ON_ERROR(convertCall(backend, node));
            break;

        case OP_RET:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting function ret \n");
            RET_ON_ERROR(convertRet(backend, node));
            break;

        case OP_IN:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting in operator \n");
            RET_ON_ERROR(convertIn(backend, node));
            break;

        case OP_OUT:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting out operator \n");
            RET_ON_ERROR(convertOut(backend, node));
            break;

        case OP_TEXT:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting text operator \n");
            logPrint(L_ZERO, 1, "Warning: Text is not supported yet. Skipping this instruction \n");
            break;

        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting binary math\n");
            RET_ON_ERROR(convertBinaryArithmetic(backend, node));
            break;

        case OP_SQRT:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting unary math\n");
            RET_ON_ERROR(convertUnaryArithmetic(backend, node));
            break;

        case OP_LABRACKET: case OP_RABRACKET:
        case OP_GREAT_EQ:  case OP_LESS_EQ: case OP_EQUAL: case OP_NEQUAL:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting comparison\n");
            RET_ON_ERROR(convertComparison(backend, node));
            break;

        case OP_ASSIGN:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting assignment\n");
            RET_ON_ERROR(convertAssign(backend, node));
            break;

        case OP_IF:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting if/else\n");
            RET_ON_ERROR(convertIfElse(backend, node));
            break;

        case OP_WHILE:
            logPrint(L_ZERO, 0, "ASTtoIR: Converting while\n");
            RET_ON_ERROR(convertWhile(backend, node));
            break;

        default:
            logPrint(L_ZERO, 1, "Backend: Operator %d (%s) isn't supported yet\n", node->value.op, operators[node->value.op].dotStr);
            exit(1);
            break;
    }

    return BACKEND_SUCCESS;
}


static BackendStatus_t convertBinaryArithmetic(BackendContext_t *backend, Node_t *node) {
    assert(node);
    assert(backend);
    assert(node->type == OPERATOR);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

    IRNode_t *irNode = IRgetNewNode(backend);


    switch(node->value.op) {
        case OP_ADD: irNode->type = IR_ADD; break;
        case OP_SUB: irNode->type = IR_SUB; break;
        case OP_MUL: irNode->type = IR_MUL; break;
        case OP_DIV: irNode->type = IR_DIV; break;

        default: assert(0);
    }

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertUnaryArithmetic(BackendContext_t *backend, Node_t *node) {
    assert(node);
    assert(backend);
    assert(node->type == OPERATOR);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));

    IRNode_t *irNode = IRgetNewNode(backend);

    switch(node->value.op) {
        case OP_SQRT: irNode->type = IR_SQRT; break;

        default: assert(0);
    }

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertComparison(BackendContext_t *backend, Node_t *node) {
    assert(node);
    assert(backend);
    assert(node->type == OPERATOR);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

    IRNode_t *irNode = IRgetNewNode(backend);

    irNode->type = IR_CMP;

    switch(node->value.op) {
        case OP_LABRACKET: irNode->cmpType = CMP_LT;  break;
        case OP_RABRACKET: irNode->cmpType = CMP_GT;  break;
        case OP_LESS_EQ:   irNode->cmpType = CMP_LE;  break;
        case OP_GREAT_EQ:  irNode->cmpType = CMP_GE;  break;
        case OP_EQUAL:     irNode->cmpType = CMP_EQ;  break;
        case OP_NEQUAL:    irNode->cmpType = CMP_NEQ; break;

        default: assert(0);
    }

    return BACKEND_SUCCESS;
}


static BackendStatus_t convertSeparator(BackendContext_t *backend, Node_t *node) {
    assert(backend);
    assert(node);

    IRComment(backend, "Semicolon %d", backend->operatorCounter++);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));
    if (node->right)
        RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertAssign(BackendContext_t *backend, Node_t *node) {
    assert(backend);
    assert(node);

    // rvalue
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

    // popping rvalue to lvalue
    IRNode_t *irNode = IRnodeCtor(backend, IR_POP);
    irNode->pushType = POP_MEM;

    int nameId = node->left->value.id;
    RET_ON_ERROR(LocalsStackSearchAddr(nameId, backend, irNode));

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertIfElse(BackendContext_t *backend, Node_t *node) {
    assert(backend);
    assert(node);

    LocalsStackInitScope(&backend->stk, NORMAL_SCOPE);

    int currentIf = backend->ifCounter++;
    // Converting condinional part of the if
    IRComment(backend, "if %d: conditional", currentIf);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));
    // Evaluated conditional is on top of the stack

    bool hasElse = cmpOp(node->right, OP_ELSE);

    if (!hasElse) {
        // Jump to the end on condition fail
        IRNode_t *endJump = IRnodeCtor(backend, IR_JZ);

        // Statement
        IRprintf(backend, "if %d: statement", currentIf);
        RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

        // End label
        uint32_t ifEndIdx = IRcreateLabel(backend, "IF%d_END", currentIf);
        endJump->addr.offset = ifEndIdx;

    } else {
        node = node->right;

        // Jump to the else branch on condition fail
        IRNode_t *elseJump = IRnodeCtor(backend, IR_JZ);

        // If Statement
        IRprintf(backend, "if %d: statement", currentIf);
        RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));

        // Jump to the end
        IRNode_t *endJump = IRnodeCtor(backend, IR_JMP);

        // Else label
        uint32_t elseLabelIdx = IRcreateLabel(backend, "IF%d_ELSE", currentIf);
        elseJump->addr.offset = elseLabelIdx;

        // Else statement
        RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

        // End label
        uint32_t endLabelIdx = IRcreateLabel(backend, "IF%d_END", currentIf);
        endJump->addr.offset = endLabelIdx;

    }

    LocalsStackPopScope(&backend->stk);
    return BACKEND_SUCCESS;
}

static BackendStatus_t convertWhile(BackendContext_t *backend, Node_t *node) {
    assert(backend); assert(node);

    LocalsStackInitScope(&backend->stk, NORMAL_SCOPE);

    int currentWhile = backend->whileCounter++;

    // loop start label
    uint32_t loopLabelIdx = IRcreateLabel(backend, "LOOP%d", currentWhile);

    // Translating condition
    IRprintf(backend, "While %d: conditional", currentWhile);
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));

    // Jmp to the end
    IRNode_t *endJump = IRnodeCtor(backend, IR_JZ);

    // Translating statement
    IRprintf(backend, "While %d: statement", currentWhile);
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

    // Jump to the start
    IRNode_t *jmpStart = IRnodeCtor(backend, IR_JMP);
    jmpStart->addr.offset = loopLabelIdx;

    // end label
    uint32_t loopEndLabelIdx = IRcreateLabel(backend, "LOOP%d_END", currentWhile);
    endJump->addr.offset = loopEndLabelIdx;

    LocalsStackPopScope(&backend->stk);
    return BACKEND_SUCCESS;
}

static BackendStatus_t convertVarDeclaration(BackendContext_t *backend, Node_t *node) {
    assert(backend);
    assert(node);

    Identifier_t *id = backend->nameTable.identifiers + node->left->value.id;

    LocalsStackPush(&backend->stk, node->left->value.id);

    bool local = backend->inFunction;
    if (local)
        IRprintf(backend, "Decl local var %s", id->str);
    else
        IRprintf(backend, "Decl global var %s", id->str);

    IRNode_t *irNode = IRnodeCtor(backend, IR_VAR_DECL);
    irNode->local = !backend->inFunction;


    return BACKEND_SUCCESS;
}

static BackendStatus_t convertFuncDecl(BackendContext_t *backend, Node_t *node) {
    assert(backend); assert(node);

    // initializing function scope
    if (backend->inFunction)
        SyntaxError(backend, BACKEND_NESTED_FUNC_ERROR, "Nested function declaration is forbidden\n");

    LocalsStackInitScope(&backend->stk, FUNC_SCOPE);
    backend->inFunction = true;

    const Node_t *funcHeader = node->left;
    Identifier_t *funcId = &backend->nameTable.identifiers[funcHeader->left->value.id];
    const char *funcName = funcId->str;

    // creating function label and writing it's address in IR to the nameTable
    IRComment(backend, "----------Function %s---------", funcName);
    IRNode_t *jumpDeclEnd = IRnodeCtor(backend, IR_JMP);

    uint32_t funcLabelIdx = IRcreateLabel(backend, "%s", funcName);
    IRNode_t *funcLabel = &backend->IR.nodes[funcLabelIdx];
    // Setting up function label
    funcLabel->local = false;                               // global label
    funcLabel->addr.offset = funcHeader->left->value.id;    // Index of function in nameTable

    // funcId->address = funcLabelIdx;

    /* Processing function arguments */
    // If there is more then one argument, they are separated with OP_SEP
    // argument is SEP_NODE->left
    Node_t *argPtr = funcHeader->right;
    int argNumber = 0;
    while (argPtr) {
        size_t argIdx = (cmpOp(argPtr, OP_COMMA)) ?  argPtr->left->value.id :
                                                   argPtr->value.id;

        Identifier_t *id = backend->nameTable.identifiers + argIdx;
        logPrint(L_EXTRA, 0, "Argument %p in function %s: %d %s\n", argPtr, funcName, argIdx, id->str);

        LocalsStackPushFuncArg(&backend->stk, argIdx, argNumber);
        argNumber++;
        argPtr = argPtr->right;
    }

    // setting frame pointer
    IRnodeCtor(backend, IR_SET_FRAME_PTR);

    // Converting code
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));
    // End of function label
    uint32_t declEndLabel = IRcreateLabel(backend, "%s_DECL_END", funcName);
    jumpDeclEnd->addr.offset = declEndLabel;

    IRComment(backend, "----------%s end---------------", funcName);

    backend->inFunction = false;

    LocalsStackPopScope(&backend->stk);

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertRet(BackendContext_t *backend, Node_t *node) {
    assert(backend); assert(node);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));

    if (backend->mode == BACKEND_TAXES)
        TODO("Imlpement taxes in function return");

    IRnodeCtor(backend, IR_RET);


    return BACKEND_SUCCESS;
}

static BackendStatus_t convertCallArguments(BackendContext_t *backend, Node_t *node, uint32_t expectedArgsCount);

static BackendStatus_t convertCall(BackendContext_t *backend, Node_t *node) {
    assert(backend); assert(node);

    Identifier_t func = getIdFromTable(&backend->nameTable, node->left->value.id);
    if (func.type != FUNC_ID)
        SyntaxError(backend, BACKEND_TYPE_ERROR, "Try to use variable %s as a function\n", func.str);

    if (node->right) {
        BackendStatus_t status = convertCallArguments(backend, node->right, func.argsCount);
        if (status != BACKEND_SUCCESS) {
            logPrint(L_ZERO, 1, "While converting call '%s'\n", func.str);
            return status;
        }
    }

    IRprintf(backend, "%s", func.str);
    IRNode_t *callNode = IRnodeCtor(backend, IR_CALL);

    if (cmpOp(node->parent, OP_SEP)) {
        // TODO("Calls without assignment are not supported yet");
    } else {
        IRprintf(backend, "Pushing call result");
        IRNode_t *pushResult = IRnodeCtor(backend, IR_PUSH);
        pushResult->pushType = PUSH_REG;
    }

    callNode->addr.offset = node->left->value.id; // id of function in nameTable

    return BACKEND_SUCCESS;
}

//! This method might not be compatible with SPU
static BackendStatus_t convertCallArguments(BackendContext_t *backend, Node_t *node, uint32_t expectedArgsCount) {
    assert(backend); assert(node);

    if (expectedArgsCount == 0)
        SyntaxError(backend, BACKEND_WRONG_ARGS_NUMBER, "Less arguments than expected\n");

    // evaluating arguments and pushing them right-to-left
    if (cmpOp(node, OP_COMMA)) {
        RET_ON_ERROR(convertCallArguments(backend, node->right, expectedArgsCount - 1));
        RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));
    } else {
        if (expectedArgsCount != 1)
            SyntaxError(backend, BACKEND_WRONG_ARGS_NUMBER, "More arguments in function call than expected: %u\n", expectedArgsCount);
        RET_ON_ERROR(convertASTtoIRrecursive(backend, node));
    }

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertIn(BackendContext_t *backend, Node_t *node) {
    assert(backend); assert(node);

    int stdlibInIdx = findIdentifier(&backend->nameTable, STDLIB_IN_FUNC_NAME);

    // calling in
    IRprintf(backend, "%s", STDLIB_IN_FUNC_NAME);
    IRNode_t *callNode = IRnodeCtor(backend, IR_CALL);
    callNode->addr.offset = stdlibInIdx;

    //TODO: maybe rework it
    //? Should I push result of the function right after the call?
    IRNode_t *resultPush = IRnodeCtor(backend, IR_PUSH);
    resultPush->pushType = PUSH_REG; // pushing rax

    // popping rvalue to lvalue
    IRNode_t *irNode = IRnodeCtor(backend, IR_POP);
    irNode->pushType = POP_MEM;

    int nameId = node->left->value.id;
    RET_ON_ERROR(LocalsStackSearchAddr(nameId, backend, irNode));
    return BACKEND_SUCCESS;
}

static BackendStatus_t convertOut(BackendContext_t *backend, Node_t *node) {
    assert(backend); assert(node);

    // converting expression
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));

    int stdlibOutIdx = findIdentifier(&backend->nameTable, STDLIB_OUT_FUNC_NAME);

    IRprintf(backend, "%s", STDLIB_OUT_FUNC_NAME);
    IRNode_t *callNode = IRnodeCtor(backend, IR_CALL);
    callNode->addr.offset = stdlibOutIdx;

    return BACKEND_SUCCESS;
}
