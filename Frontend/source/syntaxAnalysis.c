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

static Node_t *GetGrammar(ParseContext_t *context, LangContext_t *frontend);

FrontendStatus_t syntaxAnalysis(LangContext_t *frontend) {
    Token_t *tokens = (Token_t *)frontend->treeMemory.base;
    ParseContext_t context = {tokens, tokens, PARSE_SUCCESS};
    frontend->tree = GetGrammar(&context, frontend);

    return (frontend->tree) ? FRONTEND_SUCCESS : FRONTEND_SYNTAX_ERROR;
}
static Node_t *GetStatement(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetAssignment(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetInput(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetPrint(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetExpr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetMulPr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetPowPr(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetPrimary(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetFunc(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetIdentifier(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetNum(ParseContext_t *context, LangContext_t *frontend);

#define LOG_ENTRY() \
    logPrint(L_EXTRA, 0, "Entered %s %d:%d\n\n", __PRETTY_FUNCTION__, context->pointer->line, context->pointer->column)

#define LOG_EXIT() \
    logPrint(L_EXTRA, 0, "Exited  %s %d:%d\n\n", __PRETTY_FUNCTION__, context->pointer->line, context->pointer->column)

static bool cmpOp(const Token_t *token, enum OperatorType op) {
    assert(token);

    return (token->node.type == OPERATOR) && (token->node.value.op == op);
}

static Node_t *GetGrammar(ParseContext_t *context, LangContext_t *frontend) {
    Node_t *val = NULL;         // value that will be returned
    Node_t *current = NULL;     // current operator
    while (1) {
        Node_t *left = GetStatement(context, frontend);
        if (context->status != PARSE_SUCCESS)
            break;

        if (!cmpOp(context->pointer, OP_SEMICOLON)) {
            SyntaxError(context, frontend, NULL, "Expected OP_SEMICOLON (%%)\n");
        }
        Node_t *op = &context->pointer->node;

        op->left = left;
        left->parent = op;

        if (!val) { //setting val with first ;
            val = op;
        }

        if (current) { // updating current
            current->right = op;
            op->parent = current;
        }

        current = op;
        context->pointer++;
    }

    context->status = PARSE_SUCCESS;

    if (cmpOp(context->pointer, OP_EOF) ) {
        context->pointer++;
        return val;
    } else {
        SyntaxError(context, frontend, NULL, "GetGrammar: Expected EOF\n");
    }
}

static Node_t *GetStatement(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    Node_t *val = GetInput(context, frontend);
    if (context->status == PARSE_SUCCESS)
        return val;
    else if (context->status == HARD_ERROR)
        return NULL;
    else if (context->status == SOFT_ERROR)
        context->status = PARSE_SUCCESS;

    val = GetPrint(context, frontend);
    if (context->status == PARSE_SUCCESS)
        return val;
    else if (context->status == HARD_ERROR)
        return NULL;
    else if (context->status == SOFT_ERROR)
        context->status = PARSE_SUCCESS;

    val = GetAssignment(context, frontend);
    if (context->status != PARSE_SUCCESS) {
        return NULL;
    }

    LOG_EXIT();
    return val;
}

static Node_t *GetInput(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Node_t *current = &context->pointer->node;
    if (current->type != OPERATOR || current->value.op != OP_IN) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    context->pointer++;
    Node_t *id = GetIdentifier(context, frontend);
    if (context->status != PARSE_SUCCESS) {
        context->status = HARD_ERROR;
        SyntaxError(context, frontend, NULL, "GetInput: expected identifier");
    }
    current->left = id;
    id->parent = current;

    LOG_EXIT();
    return current;
}

static Node_t *GetPrint(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Node_t *current = &context->pointer->node;
    if (current->type != OPERATOR || current->value.op != OP_OUT) {
        context->status = SOFT_ERROR;
        return NULL;
    }

    context->pointer++;
    Node_t *expr = GetExpr(context, frontend);
    if (context->status != PARSE_SUCCESS) {
        context->status = HARD_ERROR;
        SyntaxError(context, frontend, NULL, "GetInput: expected expression");
    }
    current->left = expr;
    expr->parent = current;

    LOG_EXIT();
    return current;
}

static Node_t *GetAssignment(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();

    Node_t *left = GetIdentifier(context, frontend);
    // DUMP_TREE(frontend, left, 0);

    if (context->status != PARSE_SUCCESS)
        return NULL;

    if (!cmpOp(context->pointer, OP_ASSIGN)) {
        SyntaxError(context, frontend, NULL, "Expected OP_ASSIGN\n");
    }

    Node_t *op = &context->pointer->node;
    context->pointer++;

    Node_t *right = GetExpr(context, frontend);
    // DUMP_TREE(frontend, right, 0);

    if (context->status != PARSE_SUCCESS)
        return NULL;

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

    Node_t *left = GetMulPr(context, frontend);
    if (context->status != PARSE_SUCCESS)
        return left;

    while (cmpOp(context->pointer, OP_ADD) || cmpOp(context->pointer, OP_SUB)) {
        Node_t *op = &context->pointer->node;

        context->pointer++;
        Token_t *lastToken = context->pointer;

        Node_t *right = GetMulPr(context, frontend);

        if (context->status != PARSE_SUCCESS) {
            SyntaxTokenError(lastToken, frontend, NULL, "Expected expression with priority >= +-\n");
        }

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
    if (context->status != PARSE_SUCCESS)
        return left;

    while (cmpOp(context->pointer, OP_MUL) || cmpOp(context->pointer, OP_DIV)) {
        Node_t *op = &context->pointer->node;

        context->pointer++;
        Token_t *lastToken = context->pointer;

        Node_t *right = GetPowPr(context, frontend);

        if (context->status != PARSE_SUCCESS) {
            SyntaxTokenError(lastToken, frontend, NULL, "Expected expression with ^\n");
        }

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
    if (context->status != PARSE_SUCCESS)
        return left;

    if (cmpOp(context->pointer, OP_POW)) {
        Node_t *op = &context->pointer->node;
        context->pointer++;

        Token_t *lastToken = context->pointer;

        Node_t *right = GetPowPr(context, frontend);
        if (context->status != PARSE_SUCCESS)
            SyntaxTokenError(lastToken, frontend, NULL, "Expected PowPr expression\n");


        op->left = left;
        op->right = right;
        left->parent = op;
        right->parent = op;
        left = op;
    }

    LOG_EXIT();
    return left;
}

static Node_t *GetPrimary(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();


    if (cmpOp(context->pointer, OP_LBRACKET) ) {
        context->pointer++;

        Node_t *val = GetExpr(context, frontend);
        if (context->status != PARSE_SUCCESS)
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
            val = GetFunc(context, frontend);
            break;
        case IDENTIFIER:
            val = GetIdentifier(context, frontend);
            break;
        default:
            assert(0);
    }

    if (context->status != PARSE_SUCCESS)
        SyntaxError(context, frontend, val, "GetPrimary: expected (expr), function(), Variable or Number, got neither\n");

    LOG_EXIT();
    return val;

}


static Node_t *GetFunc(ParseContext_t *context, LangContext_t *frontend) {
    LOG_EXIT();

    Node_t *op = &context->pointer->node;

    context->pointer++;
    // Token_t *lastToken = context->pointer;

    if (!cmpOp(context->pointer, OP_LBRACKET) ) {
        SyntaxError(context, frontend, NULL, "GetFunc: expected '('\n");
    }

    context->pointer++;

    Node_t *val = GetExpr(context, frontend);

    if (context->status != PARSE_SUCCESS)
        return NULL;

    if (!cmpOp(context->pointer, OP_RBRACKET) ) {
        SyntaxError(context, frontend, NULL, "GetFunc: expected ')'\n");
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
    frontend->nameTable.identifiers[val->value.id].type = VAR_ID;
    context->pointer++;
    logPrint(L_EXTRA, 0, "Identifier %s\n", getIdentifier(&frontend->nameTable, val->value.id).str);
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
