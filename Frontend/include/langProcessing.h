#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

enum ParseStatus {
    SOFT_ERROR,
    HARD_ERROR,
    PARSE_SUCCESS
};

typedef struct {
    Token_t *text;
    Token_t *pointer;      // current position in str
    enum ParseStatus status;
} ParseContext_t;

/*
Positive examples = {x = 3$ + 2₽ - x^(3+2) % Invest x % ShowTaxes x}
Negative examples = {y, sin(), 5.23 , Invest x+5}

Grammar::= [Statement %]+ EOF
Statement ::= Input | Print | Assignment
Print  ::= "ShowBalance" Expr
Input  ::= "Invest" Identifier
Assignment ::=  Identifier '=' Expr
Expr   ::=MulPr{ ['+''-']  MulPr}*
MulPr  ::=PowPr{ ['*''/']  PowPr}*
PowPr  ::=Primary{ '^'  PowPr}?

Primary::= '(' Expr ')' | Func | Var | Num

Func       ::=["sin" "cos" "tg" "ctg" "ln"]'(' Expr ')'
Identifier ::=['a'-'z''_']+ ['a'-'z''_''0'-'9']*

Num    ::= [number][₽$]
*/

#define SyntaxError(context, frontend, ret, ...)                            \
    do {                                                                    \
        context->status = HARD_ERROR;                                       \
        logPrint(L_ZERO, 1, "Error while parsing context[%p]\n", context);  \
        logPrint(L_ZERO, 1, "Current position: %s:%d:%d\n",                 \
            frontend->inputFileName, context->pointer->line, context->pointer->column);  \
        logPrint(L_ZERO, 1, __VA_ARGS__);                                   \
        return ret;                                                         \
    } while(0)

#define SyntaxTokenError(token, frontend, ret, ...)                         \
    do {                                                                    \
        context->status = HARD_ERROR;                                       \
        logPrint(L_ZERO, 1, "Error while parsing\n");                       \
        logPrint(L_ZERO, 1, "Current position: %s:%d:%d\n",                 \
            frontend->inputFileName, token->line, token->column);           \
        logPrint(L_ZERO, 1, __VA_ARGS__);                                   \
        return ret;                                                         \
    } while(0)


#endif
