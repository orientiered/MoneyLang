#include <stdio.h>
#include <logger.h>
#include <assert.h>

#include "backend.h"

#define CALLOC(elemNumber, Type) (Type *) calloc(elemNumber, sizeof(Type))
#define TODO(msg) do {fprintf(stderr, "TODO: %s at %s:%d\n", msg, __FILE__, __LINE__); abort(); } while(0);

/* ================ IR functions      ================= */

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

//TODO: create function with same functionality
#define IRComment(backend, ...) \
    do { \
        IR_t *ir = &backend->IR;                            \
        assert(ir->size < IR_MAX_SIZE);                     \
        IRNode_t *node = &ir->nodes[ir->size++];            \
        node->type = IR_NOP;                                \
        node->comment = ir->commentsPtr;                    \
        ir->commentsPtr += sprintf(ir->commentsPtr, __VA_ARGS__);\
    } while(0)


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
            logPrint(L_EXTRA, 0, "Translating id %d %s\n", id, tableId.str);

            TODO("Implement variable pushing from locals stack");
            // if (local)
            //     fprintf(file, "[rbx + %d] ; local %s\n", currentVar.address, tableId.str);
            // else {
            //     fprintf(file, "[%d] ; global %s\n", currentVar.address, tableId.str);
            // }

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



BackendStatus_t convertASTtoIR(BackendContext_t *backend, Node_t *ast) {
    assert(backend);
    assert(ast);

    IR_t ir = IRCtor(IR_MAX_SIZE);
    backend->IR = ir;

    RET_ON_ERROR(convertASTtoIRrecursive(backend, ast));

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertASTtoIRrecursive(BackendContext_t *backend, Node_t *node) {
    assert(backend);
    assert(node);

    IR_t *IR = &backend->IR;
    assert(IR->size < IR->capacity);

    IRNode_t *irNode = &IR->nodes[IR->size];

    if (node->type == NUMBER) {
        IR->size++;
        irNode->type = IR_PUSH;
        irNode->extra = PUSH_IMM;

        irNode->dval = node->value.number;

        return BACKEND_SUCCESS;
    }

    if (node->type == IDENTIFIER) {
        IR->size++;
        irNode->type = IR_PUSH;
        irNode->extra = PUSH_MEM;

        int nameId = node->value.id;

        RET_ON_ERROR(LocalsStackSearchAddr(nameId, backend, irNode));
        return BACKEND_SUCCESS;
    }


    switch(node->value.op) {
        case OP_SEP:
            convertSeparator(backend, node);
            break;

        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
            convertBinaryArithmetic(backend, node);
            break;

        case OP_SQRT:
            convertUnaryArithmetic(backend, node);
            break;

        case OP_LABRACKET: case OP_RABRACKET:
        case OP_GREAT_EQ:  case OP_LESS_EQ: case OP_EQUAL: case OP_NEQUAL:
            convertComparison(backend, node);
            break;

        case OP_ASSIGN:
            convertAssign(backend, node);
            break;

        default:
            logPrint(L_ZERO, 1, "Backend: Operator %d (%s) isn't supported yet\n", node->value.op, operators[node->value.op].dotStr);
            exit(1);
    }

    return BACKEND_ERROR;
}



static BackendStatus_t convertBinaryArithmetic(BackendContext_t *backend, Node_t *node) {
    assert(node);
    assert(backend);
    assert(node->type == OPERATOR);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

    IR_t *IR = &backend->IR;
    assert(IR->size < IR->capacity);

    IRNode_t *irNode = &IR->nodes[IR->size];

    switch(node->value.op) {
        case OP_ADD: irNode->type = IR_ADD; break;
        case OP_SUB: irNode->type = IR_SUB; break;
        case OP_MUL: irNode->type = IR_MUL; break;
        case OP_DIV: irNode->type = IR_DIV; break;

        default: assert(0);
    }

    IR->size++;

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertUnaryArithmetic(BackendContext_t *backend, Node_t *node) {
    assert(node);
    assert(backend);
    assert(node->type == OPERATOR);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));

    IR_t *IR = &backend->IR;
    assert(IR->size < IR->capacity);

    IRNode_t *irNode = &IR->nodes[IR->size];

    switch(node->value.op) {
        case OP_SQRT: irNode->type = IR_SQRT; break;

        default: assert(0);
    }

    IR->size++;

    return BACKEND_SUCCESS;
}

static BackendStatus_t convertComparison(BackendContext_t *backend, Node_t *node) {
    assert(node);
    assert(backend);
    assert(node->type == OPERATOR);

    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->left));
    RET_ON_ERROR(convertASTtoIRrecursive(backend, node->right));

    IR_t *IR = &backend->IR;
    assert(IR->size < IR->capacity);

    IRNode_t *irNode = &IR->nodes[IR->size];

    switch(node->value.op) {
        case OP_LABRACKET: irNode->type = IR_CMPLT;  break;
        case OP_RABRACKET: irNode->type = IR_CMPGT;  break;
        case OP_LESS_EQ:   irNode->type = IR_CMPLE;  break;
        case OP_GREAT_EQ:  irNode->type = IR_CMPGE;  break;
        case OP_EQUAL:     irNode->type = IR_CMPEQ;  break;
        case OP_NEQUAL:    irNode->type = IR_CMPNEQ; break;

        default: assert(0);
    }

    IR->size++;

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
    IR_t *ir = &backend->IR;
    assert(ir->size < ir->capacity);
    IRNode_t *irNode = ir->nodes + ir->size;

    irNode->type = IR_POP;
    irNode->extra = POP_MEM;

    int nameId = node->left->value.id;
    RET_ON_ERROR(LocalsStackSearchAddr(nameId, backend, irNode));

    ir->size++;

    return BACKEND_SUCCESS;
}
