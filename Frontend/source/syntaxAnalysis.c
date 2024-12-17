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


static Node_t *GetBlock(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetStatement(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetIf(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetWhile(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetAssignment(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetInput(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetPrint(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetExpr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetAddPr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetMulPr(ParseContext_t *context, LangContext_t *frontend);
static Node_t *GetPowPr(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetPrimary(ParseContext_t *context, LangContext_t *frontend);

static Node_t *GetFunc(ParseContext_t *context, LangContext_t *frontend);
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

static Node_t *GetGrammar(ParseContext_t *context, LangContext_t *frontend) {
    Node_t *val = NULL;         // value that will be returned
    Node_t *current = NULL;     // current operator
    while (1) {
        Node_t *right = GetBlock(context, frontend);
        if (context->status != PARSE_SUCCESS)
            break;

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
        SyntaxError(context, frontend, NULL, "GetGrammar: Expected EOF\n");
    }
}


static Node_t *GetIf(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    if (!cmpOp(context->pointer, OP_IF)) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *val = &context->pointer->node;
    context->pointer++;

    Node_t *left = GetExpr(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected expression after if\n");

    if (!cmpOp(context->pointer, OP_ARROW))
        SyntaxError(context, frontend, NULL, "Expected -> in if statement\n");
    Node_t *semicolon = &context->pointer->node;
    semicolon->value.op = OP_SEMICOLON;
    context->pointer++;

    Node_t *right = GetBlock(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected code block after -> in if statement\n");

    semicolon->left = val;
    val->parent = semicolon;

    val->left = left;
    val->right = right;
    left->parent = val;
    right->parent = val;
    return semicolon;
}

static Node_t *GetWhile(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    if (!cmpOp(context->pointer, OP_WHILE)) {
        context->status = SOFT_ERROR;
        return NULL;
    }
    Node_t *val = &context->pointer->node;
    context->pointer++;

    Node_t *left = GetExpr(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected expression after while\n");

    if (!cmpOp(context->pointer, OP_ARROW))
        SyntaxError(context, frontend, NULL, "Expected -> in while statement\n");
    Node_t *semicolon = &context->pointer->node;
    semicolon->value.op = OP_SEMICOLON;
    context->pointer++;

    Node_t *right = GetBlock(context, frontend);
    if (!SUCCESS)
        SyntaxError(context, frontend, NULL, "Expected code block after -> in while statement\n");

    semicolon->left = val;
    val->parent = semicolon;

    val->left = left;
    val->right = right;
    left->parent = val;
    right->parent = val;
    return semicolon;
}

static Node_t *GetBlock(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    if (cmpOp(context->pointer, OP_LABRACKET)) {
        context->pointer++;

        Node_t *val = GetStatement(context, frontend);
        if (!SUCCESS) {
            context->status = HARD_ERROR;
            return NULL;
        }

        Node_t *current = val;

        while (1) {
            Node_t *right = GetStatement(context, frontend);
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
        Node_t *linker = &context->pointer->node;
        context->pointer++;
        linker->value.op = OP_SEMICOLON;
        linker->left = val;
        val->parent = linker;

        val = linker;
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
    Node_t *val = GetInput(context, frontend);
    if (SUCCESS)
        return val;
    else if (HARD_ERR)
        return NULL;
    else if (SOFT_ERR)
        context->status = PARSE_SUCCESS;

    val = GetPrint(context, frontend);
    if (SUCCESS)
        return val;
    else if (HARD_ERR)
        return NULL;
    else if (SOFT_ERR)
        context->status = PARSE_SUCCESS;

    val = GetAssignment(context, frontend);
    if (context->status != PARSE_SUCCESS) {
        return NULL;
    }

    LOG_EXIT();
    return val;
}

static Node_t *GetStatement(ParseContext_t *context, LangContext_t *frontend) {
    LOG_ENTRY();
    Node_t *val = GetStatementBase(context, frontend);
    DUMP_TREE(frontend, val, 0);

    if (SUCCESS) {
        if (!cmpOp(context->pointer, OP_SEMICOLON))
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

    if (SOFT_ERR) {
        context->status = PARSE_SUCCESS;
        val = GetIf(context, frontend);
        if (SUCCESS)
            return val;
        if (HARD_ERR)
            return NULL;
        else
            context->status = PARSE_SUCCESS;

        val = GetWhile(context, frontend);
        if (SUCCESS)
            return val;
    }

    return NULL;
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

    Node_t *left = GetAddPr(context, frontend);
    if (context->status != PARSE_SUCCESS)
        return left;

    while (cmpOp(context->pointer, OP_RABRACKET) || cmpOp(context->pointer, OP_LABRACKET)) {
        Node_t *op = &context->pointer->node;

        context->pointer++;
        Token_t *lastToken = context->pointer;

        Node_t *right = GetAddPr(context, frontend);

        if (context->status != PARSE_SUCCESS)
            SyntaxTokenError(lastToken, frontend, NULL, "Expected expression after > or <\n");

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
        Token_t *lastToken = context->pointer;

        Node_t *right = GetMulPr(context, frontend);

        if (context->status != PARSE_SUCCESS) {
            SyntaxTokenError(lastToken, frontend, NULL, "Expected expression after + or - \n");
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
            SyntaxTokenError(lastToken, frontend, NULL, "Expected expression after * or /\n");
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
            SyntaxTokenError(lastToken, frontend, NULL, "Expected expression after ^\n");

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
