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
Positive examples = {x = 3$ + 2₽ - x^(3+2) % Invest x % ShowTaxes x
if x+5 -> y = -5 %
if y > x ->
<
    Invest x %
    ShowTaxes y%
>
Transaction x -> mean -> Pay (x + y) / 2 %

Invest x if y %}
Negative examples = {y, sin(), 5.23 , Invest x+5}

Grammar::= [FunctionDecl | Block ]+ EOF
FunctionDecl::= "Transaction" IdChain '->' Identifier '->' Block
Block  ::= "<" Block+ ">" | Statement
Statement ::= [Input | Print | Pay | Text | VarDecl | FunctionCall | Assignment] % | If | While
Text   ::= "Txt"  '"'String'"'
If     ::= "if" Expr -> Block Else?
Else   ::= "else" BLock
While  ::= "while" Expr -> Block
Print  ::= "ShowBalance" Expr
Pay    ::= "Pay" Expr
Input  ::= "Invest" Identifier
VarDecl ::= "Account" Identifier
Assignment ::= Identifier '=' Expr

Expr   ::=AddPr{ ['>''<' '>==' '==<' '===' '!=='] AddPr}*
AddPr  ::=MulPr{ ['+''-']  MulPr}*
MulPr  ::=PowPr{ ['*''/']  PowPr}*
PowPr  ::=Primary{ '^'  PowPr}?


//NOTE: IdChain and ExprChain are represented with GetIdOrExprChain
IdChain   ::= Identifier?[,Identifier]*
ExprChain ::= Expr?[,Expr]*

FunctionCall::= Identifier'('ExprChain')'
Primary::= '(' Expr ')' | FuncOper | FunctionCall | Identifier | Num

FuncOper   ::=["sin" "cos" "tg" "ctg" "ln"]'(' Expr ')'
Identifier ::=['a'-'z''_']+ ['a'-'z''_''0'-'9']*
String     ::=[^"]
Num    ::= [number][₽$]
*/

void printPositionInText(ParseContext_t *context);

#define SyntaxError(context, frontend, ret, ...)                            \
    do {                                                                    \
        /*skipping error, because status is already set*/                   \
        if (context->status == HARD_ERROR) return ret;                      \
        context->status = HARD_ERROR;                                       \
        logPrint(L_ZERO, 1, "Error while parsing context[%p]\n", context);  \
        logPrint(L_ZERO, 1, "Current position: %s:%d:%d\n",                 \
            frontend->inputFileName, context->pointer->line, context->pointer->column);  \
        printPositionInText(context);                                       \
        logPrint(L_ZERO, 1, __VA_ARGS__);                                   \
        return ret;                                                         \
    } while(0)

#endif
