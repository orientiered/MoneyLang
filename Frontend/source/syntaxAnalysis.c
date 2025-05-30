#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#include "logger.h"
#include "nameTable.h"
#include "frontend.h"
#include "langProcessing.h"


static void setIdType(LangContext_t *frontend, Node_t *node, enum IdentifierType type) {
    assert(frontend);
    assert(node);
    assert(node->type == IDENTIFIER);
    logPrint(L_EXTRA, 0, "Setting type of id %d: %d\n", node->value.id, type);
    frontend->nameTable.identifiers[node->value.id].type = type;
}

static void setArgsCount(LangContext_t *frontend, Node_t *node, size_t argsCount) {
    assert(frontend);
    assert(node);
    assert(node->type == IDENTIFIER);
    frontend->nameTable.identifiers[node->value.id].argsCount = argsCount;
}

static Node_t *GetGrammar(ParseContext_t *context, LangContext_t *frontend);

FrontendStatus_t syntaxAnalysis(LangContext_t *frontend) {
    Token_t *tokens = (Token_t *)frontend->treeMemory.base;
    ParseContext_t context = {tokens, tokens, PARSE_SUCCESS};
    frontend->tree = GetGrammar(&context, frontend);

    return (frontend->tree) ? FRONTEND_SUCCESS : FRONTEND_SYNTAX_ERROR;
}

static Node_t *GetFunctionDecl(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetBlock(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetStatement(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetIf(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetElse(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetWhile(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetAssignment(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetFunctionCall(ParseContext_t *context, LangContext_t *frontend);
// get arguments in format (expr?[,expr]*)
static Node_t *GetIdOrExprChain(ParseContext_t *context, LangContext_t *frontend, bool expr, size_t *argsCount);
static Node_t *GetFunctionArgs(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetText(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetInput(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetPrint(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetReturn(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetVarDecl(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetExpr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetAddPr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetMulPr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetPowPr(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetPrimary(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetFuncOper(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetIdentifier(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetNum(ParseContext_t *context, LangContext_t *frontend);

#define LOG_ENTRY() \
    logPrint(L_EXTRA, 0, "Entered %s %d:%d\n\n", __PRETTY_FUNCTION__, context->pointer->line, context->pointer->column)

#define LOG_EXIT() ;
    //logPrint(L_EXTRA, 0, "Exited  %s %d:%d\n\n", __PRETTY_FUNCTION__, context->pointer->line, context->pointer->column)

#define SUCCESS (context->status == PARSE_SUCCESS)
#define SOFT_ERR (context->status == SOFT_ERROR)
#define HARD_ERR (context->status == HARD_ERROR)

static bool cmpOp(const Token_t *token, enum OperatorType op) {
    assert(token);

    return (token->node.type == OPERATOR) && (token->node.value.op == op);
}

void printPositionInText(ParseContext_t *context) {

    Token_t *token = context->pointer;
    if (context->pointer > context->text) token = context->pointer - 1; // if possible, showing previous token

    const char *line = token->pos;
    if (token->column > 0)
        line -= token->column - 1;
    while (*line != '\n' && *line) {
        logPrint(L_ZERO, 1, "%c", *line);
        line++;
    }
    logPrint(L_ZERO, 1, "\n");
    for (int idx = 0; idx < (int)token->column - 2; idx++) {
        logPrint(L_ZERO, 1, " ");
    }
    logPrint(L_ZERO, 1, "^^^\n");
}

static Node_t *GetGrammar(ParseContext_t *context, LangContext_t *frontend) {
    context->status = PARSE_SUCCESS;

    Node_t *val = NULL;         // value that will be returned
    Node_t *current = NULL;     // current operator
    while (1) {
        Node_t *right = GetFunctionDecl(context, frontend);
        if (HARD_ERR)
            return NULL;
        else if (SOFT_ERR) {
            right = GetBlock(context, frontend);

            if (HARD_ERR)
                return NULL;
            else if (SOFT_ERR)
                break;
        }

        if (!val) { //setting val with first ;
            val = right;
        }

        if (current) { // updating current
            current->right = right;
            right->parent = current;
        }

        current = right;
        DUMP_TREE(frontend, val, 0);
    }

    context->status = PARSE_SUCCESS;

    if (cmpOp(context->pointer, OP_EOF) ) {
        context->pointer++;
        return val;
    } else {
        SyntaxError(context, frontend, NULL, "GetGrammar: Expected EOF\n"
                                             "Probably you have misspelled language keyword\n");
    }
}


static Node_t *GetFunctionDecl(ParseContext_t *context, LangContext_t *frontend) {
    assert(context);
    assert(frontend);
    LOG_ENTRY();

    context->status = PARSE_SUCCESS;
/* Transaction  x      ->         sqr       ->        Pay x * x %
    declNode  args  headerNode  funcName  linker(;)    body

linker is used to connect declNode the same way as other statements:
            ;
declNode        NULL

*/
    if (!cmpOp(context->pointer, OP_FUNC_DECL)) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *declNode = &context->pointer->node;
    context->pointer++;

    size_t argsCount = 0;
    Node_t *args = GetIdOrExprChain(context, frontend, 0, &argsCount);
    if (HARD_ERR)
        return NULL;

    if (!cmpOp(context->pointer, OP_ARROW) )
        SyntaxError(context, frontend, NULL, "Expected -> in function decl\n");
    //transforming -> into OP_FUNC_HEADER
    Node_t *headerNode = &context->pointer->node;
    headerNode->value.op = OP_FUNC_HEADER;
    context->pointer++;

    declNode->left = headerNode;
    headerNode->parent = declNode;

    headerNode->right = args;
    if (args) args->parent = headerNode;

    Node_t *funcName = GetIdentifier(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected name of function after ->\n");

    headerNode->left = funcName;
    funcName->parent = headerNode;

    setIdType(frontend, funcName, FUNC_ID);
    setArgsCount(frontend, funcName, argsCount);

    if (!cmpOp(context->pointer, OP_ARROW))
        SyntaxError(context, frontend, NULL, "Expected -> in function decl\n");
    //transforming second -> into ; to connect function decl to chain
    Node_t *linker = &context->pointer->node;
    linker->value.op = OP_SEP;
    context->pointer++;

    Node_t *body = GetBlock(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected code block in function declaration\n");

    declNode->right = body;
    body->parent = declNode;

    linker->left = declNode;
    declNode->parent = linker;
    return linker;
}

static Node_t *GetText(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    context->status = PARSE_SUCCESS;

    if (!cmpOp(context->pointer, OP_TEXT)) {
        context->status = SOFT_ERROR;
        return NULL;
    }

    Node_t *textNode = &context->pointer->node;
    context->pointer++;

    if (!cmpOp(context->pointer, OP_QUOTE) )
        SyntaxError(context, frontend, NULL, "Expected \" after text operator\n");

    context->pointer++;
    Node_t *textId = GetIdentifier(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected id after quote\n");

    if (!cmpOp(context->pointer, OP_QUOTE) )
        SyntaxError(context, frontend, NULL, "Expected \" after identifier in text operator\n");
    context->pointer++;

    textNode->left = textId;
    textId->parent = textNode;

    return textNode;
}

static Node_t *GetIf(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    context->status = PARSE_SUCCESS;

    if (!cmpOp(context->pointer, OP_IF)) {
        context->status = SOFT_ERROR;
        return NULL;
    }
//   if expr     ->      Block
//  val left semicolon    body
    Node_t *val = &context->pointer->node;
    context->pointer++;

    Node_t *left = GetExpr(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected expression after if\n");

    if (!cmpOp(context->pointer, OP_ARROW))
        SyntaxError(context, frontend, NULL, "Expected -> in if statement\n");
    Node_t *semicolon = &context->pointer->node;
    semicolon->value.op = OP_SEP;
    context->pointer++;

    Node_t *body = GetBlock(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected code block after -> in if statement\n");

    Node_t *elseNode = GetElse(context, frontend);
    if (HARD_ERR)
        return NULL;

    semicolon->left = val;
    val->parent = semicolon;

    val->left = left;
    left->parent = val;

    if (SUCCESS) {
        elseNode->left = body;
        body->parent = elseNode;

        val->right = elseNode;
        elseNode->parent = val;
    } else {
        context->status = PARSE_SUCCESS;
        val->right = body;
        body->parent = val;
    }

    return semicolon;
}

static Node_t *GetElse(ParseContext_t *context, LangContext_t *frontend) {
    assert(context);
    assert(frontend);
    LOG_ENTRY();

    if (!cmpOp(context->pointer, OP_ELSE) ) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *elseNode = &context->pointer->node;
    context->pointer++;

    Node_t *elseBody = GetBlock(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected code block after else statement\n");

    elseNode->right = elseBody;
    elseBody->parent = elseNode;

    return elseNode;
}


static Node_t *GetWhile(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    context->status = PARSE_SUCCESS;

    if (!cmpOp(context->pointer, OP_WHILE)) {
        context->status = SOFT_ERROR;
        return NULL;
    }
// while expr    ->      block
//  val  left semicolon   body
    Node_t *val = &context->pointer->node;
    context->pointer++;

    Node_t *left = GetExpr(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected expression after while\n");

    if (!cmpOp(context->pointer, OP_ARROW))
        SyntaxError(context, frontend, NULL, "Expected -> in while statement\n");
    Node_t *semicolon = &context->pointer->node;
    semicolon->value.op = OP_SEP;
    context->pointer++;

    Node_t *body = GetBlock(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected code block after -> in while statement\n");

    semicolon->left = val;
    val->parent = semicolon;

    val->left = left;
    val->right = body;
    left->parent = val;
    body->parent = val;
    return semicolon;
}

static Node_t *GetBlock(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    context->status = PARSE_SUCCESS;

    if (cmpOp(context->pointer, OP_LABRACKET)) {
        context->pointer++;

        Node_t *val = GetBlock(context, frontend);
        if (!SUCCESS)
            SyntaxError(context, frontend, NULL, "Expected code block after '<'\n");

        Node_t *current = val;

        while (1) {
            Node_t *right = GetBlock(context, frontend);
            if (SOFT_ERR)
                break;
            else if (HARD_ERR)
                return NULL;

            current->right = right;
            right->parent = current;

            current = right;
        }
        context->status = PARSE_SUCCESS;

        if (!cmpOp(context->pointer, OP_RABRACKET)) {
            SyntaxError(context, frontend, NULL, "Expected '>'\n");
        }
        context->pointer++;
        LOG_EXIT();
        // DUMP_TREE(frontend, val, 0);
        return val;
    }

    Node_t *val = GetStatement(context, frontend);
    if (!SUCCESS)
        return NULL;

    // DUMP_TREE(frontend, val, 0);
    return val;
}

static Node_t *GetStatementBase(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    typedef Node_t *(*SyntaxFunc_t)(ParseContext_t *, LangContext_t *);

    const SyntaxFunc_t Getters[] = {
        GetInput,
        GetPrint,
        GetReturn,
        GetText,
        GetVarDecl,
        GetFunctionCall,
        GetAssignment
    };

    Node_t *val = NULL;
    for (size_t funcIdx = 0; funcIdx < ARRAY_SIZE(Getters); funcIdx++) {
        val = Getters[funcIdx](context, frontend);
        if (SUCCESS)
            return val;
        if (HARD_ERR)
            return NULL;
        context->status = PARSE_SUCCESS;
    }
//TODO: move context->status = PARSE_SUCCESS to start of every function
    context->status = SOFT_ERROR;
    return NULL;
}

static Node_t *GetFunctionCall(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    //entry token to return it back if it is not function call
    Token_t *currentToken = context->pointer;
    Node_t *name = GetIdentifier(context, frontend);
    if (!SUCCESS)
        return NULL;

    //return call node with arguments linked to right subtree
    if (!cmpOp(context->pointer, OP_LBRACKET) ) {
        context->pointer = currentToken;
        context->status = SOFT_ERROR;
        return NULL;
    }
    context->pointer++;

    size_t argsCount = 0;
    Node_t *args = GetIdOrExprChain(context, frontend, true, &argsCount);
    if (!SUCCESS)
        return NULL;

    if (!cmpOp(context->pointer, OP_RBRACKET) )
        SyntaxError(context, frontend, NULL, "Expected )\n");
    Node_t *callNode = &context->pointer->node;
    context->pointer++;
    //linking name of function to left subtree
    callNode->value.op = OP_CALL;
    callNode->left = name;
    name->parent = callNode;
    callNode->right = args;
    if (args) args->parent = callNode;
    return callNode;
}

static Node_t *GetStatement(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    Node_t *val = GetStatementBase(context, frontend);
    DUMP_TREE(frontend, val, 0);

    if (SUCCESS) {
        if (!cmpOp(context->pointer, OP_SEP))
            SyntaxError(context, frontend, NULL, "Expected %%\n");

        Node_t *op = &context->pointer->node;
        context->pointer++;

        op->left = val;
        val->parent = op;
        val = op;
        // DUMP_TREE(frontend, op, 0);

        LOG_EXIT();
        return val;
    }

    if (HARD_ERR) return NULL;
    context->status = PARSE_SUCCESS;

    val = GetIf(context, frontend);
    if (SUCCESS)  return val;
    if (HARD_ERR) return NULL;
    context->status = PARSE_SUCCESS;

    val = GetWhile(context, frontend);
    if (SUCCESS)  return val;

    return NULL;
}

static Node_t *GetInput(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    if (!cmpOp(context->pointer, OP_IN) ){
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *inNode = &context->pointer->node;
    context->pointer++;
    Node_t *id = GetIdentifier(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "GetInput: expected identifier");

    inNode->left = id;
    id->parent = inNode;

    LOG_EXIT();
    return inNode;
}

static Node_t *GetPrint(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    if (!cmpOp(context->pointer, OP_OUT) ) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *outNode = &context->pointer->node;

    context->pointer++;
    Node_t *expr = GetExpr(context, frontend);
    if (context->status != PARSE_SUCCESS)
        SyntaxError(context, frontend, NULL, "GetInput: expected expression");

    outNode->left = expr;
    expr->parent = outNode;

    LOG_EXIT();
    return outNode;
}

static Node_t *GetReturn(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    if (!cmpOp(context->pointer, OP_RET) ) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *retNode = &context->pointer->node;

    context->pointer++;
    Node_t *expr = GetExpr(context, frontend);
    if (context->status != PARSE_SUCCESS)
        SyntaxError(context, frontend, NULL, "GetReturn: expected expression");

    retNode->left = expr;
    expr->parent = retNode;

    LOG_EXIT();
    return retNode;
}

static Node_t *GetVarDecl(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    context->status = PARSE_SUCCESS;

    if (!cmpOp(context->pointer, OP_VAR_DECL) ) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *declNode = &context->pointer->node;
    context->pointer++;

    Node_t *id = GetIdentifier(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "GetVarDecl: Expected identifier\n");

    declNode->left = id;
    id->parent = declNode;

    setIdType(frontend, id, VAR_ID);

    return declNode;
}


static Node_t *GetAssignment(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Token_t *lastToken = context->pointer;
    Node_t *left = GetIdentifier(context, frontend);
    // DUMP_TREE(frontend, left, 0);

    if (!SUCCESS)
        return NULL;

    if (!cmpOp(context->pointer, OP_ASSIGN)) {
        context->status = SOFT_ERROR;
        context->pointer = lastToken;
        return NULL;
    }

    Node_t *op = &context->pointer->node;
    context->pointer++;

    Node_t *right = GetExpr(context, frontend);
    // DUMP_TREE(frontend, right, 0);

    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected expression after =\n");

    op->left = left;
    op->right = right;
    left->parent = op;
    right->parent = op;

    LOG_EXIT();
    // DUMP_TREE(frontend, op, 0);
    return op;
}

static Node_t *GetExpr(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Node_t *left = GetAddPr(context, frontend);
    if (context->status != PARSE_SUCCESS)
        return left;

    while (cmpOp(context->pointer, OP_RABRACKET) || cmpOp(context->pointer, OP_LABRACKET) ||
           cmpOp(context->pointer, OP_GREAT_EQ)  || cmpOp(context->pointer, OP_LESS_EQ) ||
           cmpOp(context->pointer, OP_EQUAL)     || cmpOp(context->pointer, OP_NEQUAL)) {
        Node_t *op = &context->pointer->node;

        context->pointer++;

        Node_t *right = GetAddPr(context, frontend);

        if (!SUCCESS)
            SyntaxError(context, frontend, NULL, "Expected expression after > or <\n");

        op->left = left;
        op->right = right;
        left->parent = op;
        right->parent = op;

        left = op;
    }

    LOG_EXIT();
    return left;
}

static Node_t *GetAddPr(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Node_t *left = GetMulPr(context, frontend);
    if (context->status != PARSE_SUCCESS)
        return left;

    while (cmpOp(context->pointer, OP_ADD) || cmpOp(context->pointer, OP_SUB)) {
        Node_t *op = &context->pointer->node;

        context->pointer++;

        Node_t *right = GetMulPr(context, frontend);

        if (!SUCCESS)
            SyntaxError(context, frontend, NULL, "Expected expression after + or - \n");

        op->left = left;
        op->right = right;
        left->parent = op;
        right->parent = op;

        left = op;
    }

    LOG_EXIT();
    return left;
}

static Node_t *GetMulPr(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Node_t *left = GetPowPr(context, frontend);
    if (!SUCCESS)
        return left;

    while (cmpOp(context->pointer, OP_MUL) || cmpOp(context->pointer, OP_DIV)) {
        Node_t *op = &context->pointer->node;

        context->pointer++;

        Node_t *right = GetPowPr(context, frontend);

        if (!SUCCESS)
            SyntaxError(context, frontend, NULL, "Expected expression after * or /\n");

        op->left = left;
        op->right = right;
        left->parent = op;
        right->parent = op;

        left = op;
    }

    LOG_EXIT();
    return left;

}

static Node_t *GetPowPr(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Node_t *left = GetPrimary(context, frontend);
    if (!SUCCESS)
        return left;

    if (cmpOp(context->pointer, OP_POW)) {
        Node_t *op = &context->pointer->node;
        context->pointer++;

        Node_t *right = GetPowPr(context, frontend);
        if (!SUCCESS)
            SyntaxError(context, frontend, NULL, "Expected expression after ^\n");

        op->left = left;
        op->right = right;
        left->parent = op;
        right->parent = op;
        left = op;
    }

    LOG_EXIT();
    return left;
}

/// @brief Get chain of identifier or expression connected with ,
// if expr == false, looks for chain of identifiers
static Node_t *GetIdOrExprChain(ParseContext_t *context, LangContext_t *frontend, bool expr, size_t *argsCount) {
    LOG_ENTRY();

    //function to get next identifier or expression
    Node_t *(*getter)(ParseContext_t *context, LangContext_t *frontend) =
        expr ? GetExpr : GetIdentifier;

    Node_t *val = NULL; //top ,
    Node_t *left = getter(context, frontend); // id
    Node_t *current = NULL; // current ,
    *argsCount = 0;
    if (HARD_ERR) {
        return NULL;
    } else if (SOFT_ERR) {
        context->status = PARSE_SUCCESS;
        return left;
    }

    (*argsCount)++;
    if (!expr) setIdType(frontend, left, VAR_ID);

    //one element in chain:
    if (!cmpOp(context->pointer, OP_COMMA) ) {
        return left;
    }

    //more elements
    while (1) {
        if (!cmpOp(context->pointer, OP_COMMA))
            break;
        Node_t *comma = &context->pointer->node;
        context->pointer++;

        Node_t *right = getter(context, frontend);
        if (!SUCCESS)
            SyntaxError(context, frontend, NULL, "Expected expr or id after ,\n");

/*
        ,  <--current        ,
    a     b     -->     a       , <-- comma, new current
                            b      c <--right
*/
        (*argsCount)++;
        setIdType(frontend, right, VAR_ID);
        //first iteration, when val and current = NULL
        if (!val) {
            val = comma;
            current = comma;
            current->left = left;
            left->parent = current;
            current->right = right;
            right->parent = current;
            continue;
        }

        comma->left = current->right;
        comma->right = right;
        current->right = comma;
        comma->parent = current;

        current = comma;

    }

    return val;
}

static Node_t *GetPrimary(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();


    if (cmpOp(context->pointer, OP_LBRACKET) ) {
        context->pointer++;

        Node_t *val = GetExpr(context, frontend);
        if (!SUCCESS)
            return NULL;


        if (!cmpOp(context->pointer, OP_RBRACKET))
            SyntaxError(context, frontend, NULL, "GetPrimary: expected ')'\n");
        else {
            context->pointer++;
            LOG_EXIT();
            return val;
        }
    }

    Node_t *val = NULL;
    switch (context->pointer->node.type) {
        case NUMBER:
            val = GetNum(context, frontend);
            break;
        case OPERATOR:
            val = GetFuncOper(context, frontend);
            break;
        case IDENTIFIER:
            val = GetFunctionCall(context, frontend);
            if (!SOFT_ERR)
                break;
            context->status = PARSE_SUCCESS;

            val = GetIdentifier(context, frontend);
            break;
        default:
            assert(0);
    }

    if (HARD_ERR)
        SyntaxError(context, frontend, val, "GetPrimary: expected (expr), function(), identifier (function call) or Number, got neither\n");
    if (SOFT_ERR)
        return val;

        LOG_EXIT();
    return val;

}


static Node_t *GetFuncOper(ParseContext_t *context, LangContext_t *frontend) {
    LOG_EXIT();

    Node_t *op = &context->pointer->node;
    if (!operators[op->value.op].isFunction) {
        context->status = SOFT_ERROR;
        return NULL;
    }

    context->pointer++;
    // Token_t *lastToken = context->pointer;

    if (!cmpOp(context->pointer, OP_LBRACKET) ) {
        SyntaxError(context, frontend, NULL, "GetFuncOper: expected '('\n");
    }

    context->pointer++;

    Node_t *val = GetExpr(context, frontend);

    if (context->status != PARSE_SUCCESS)
        return NULL;

    if (!cmpOp(context->pointer, OP_RBRACKET) ) {
        SyntaxError(context, frontend, NULL, "GetFuncOper: expected ')'\n");
    }
    context->pointer++;

    op->left = val;
    val->parent = op;

    LOG_EXIT();
    return op;
}

static Node_t *GetIdentifier(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    if (context->pointer->node.type != IDENTIFIER) {
        context->status = SOFT_ERROR;
        return NULL;
    }

    Node_t *val = &context->pointer->node;
    context->pointer++;
    logPrint(L_EXTRA, 0, "Identifier %s\n", getIdFromTable(&frontend->nameTable, val->value.id).str);
    LOG_EXIT();
    return val;
}

static Node_t *GetNum(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    if (context->pointer->node.type != NUMBER) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *val = &context->pointer->node;
    context->pointer++;
    Node_t *currency = &context->pointer->node;
    if (currency->type != OPERATOR || (currency->value.op != OP_DOLLAR && currency->value.op != OP_RUBLE) ) {
        SyntaxError(context, frontend, NULL, "GetNum: expected $ or ₽\n");
    }
    context->pointer++;

    if (currency->value.op == OP_DOLLAR)
        val->value.number *= DOLLAR_TO_RUBLE;

    LOG_EXIT();
    return val;
}
