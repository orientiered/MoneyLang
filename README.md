# Money++ programming language

## Table of contents

1. [MoneyLang syntax](#language-syntax)
2. [Compilation](#compilation-and-usage)
3. [Frontend](#frontend)
3. [$\text{Frontend}^{-1}$](#reverse-frontend)
4. [Middle-end](#middle-end)
5. [Backend](#backend)

## Language syntax

### Code example: calculate factorial of given number

[fact.mpp](examples/fact.mpp)

```
@ Creating variable a (all values are fixed-point floats)
Account a %

@ Declaring function fact that takes a as an input
Transaction a -> fact ->
<
    @ Comparison operators come with more = that you might have expected
    @ Also all numeric constants must end with $ or ₽.
    @ Dollars are converted to rubles in frontend with 35:1 ratio
    if a ==< 1₽ ->
        @ Pay is return
        Pay 1₽ %

    Account b %
    @ Recursion
    b = fact(a - 1₽) * a %

    Pay b %
>

@ Read value from standard input to a
Invest a %
@ Print value to standard output
ShowBalance fact(a) %
```

See other examples in `examples` folder

### EBNF parsing rules:
```
Grammar::= [FunctionDecl | Block ]+ EOF
FunctionDecl::= "Transaction" IdChain "->" Identifier "->" Block
Block  ::= "<" Block+ ">" | Statement
Statement ::= [Input | Print | Pay | Text | VarDecl | FunctionCall | Assignment] % | If | While
Text   ::= "Txt"  '"'String'"'
If     ::= "if" Expr "->" Block Else?
Else   ::= "else" BLock
While  ::= "while" Expr "->" Block
Print  ::= "ShowBalance" Expr
Pay    ::= "Pay Expr
Input  ::= "Invest" Identifier
VarDecl ::= "Account" Identifier
Assignment ::= Identifier '=' Expr

Expr   ::=AddPr{ ['>''<' '>==' '==<' '====' '!=='] AddPr}*
AddPr  ::=MulPr{ ['+''-']  MulPr}*
MulPr  ::=PowPr{ ['*''/']  PowPr}*
PowPr  ::=Primary{ '^'  PowPr}?


//NOTE: IdChain and ExprChain are represented with GetIdOrExprChain
IdChain   ::= Identifier?[','Identifier]*
ExprChain ::= Expr?[','Expr]*

FunctionCall::= Identifier'('ExprChain')'
Primary::= '(' Expr ')' | FuncOper | FunctionCall | Identifier | Num

FuncOper   ::=["sin" "cos" "tg" "ctg" "ln"]'(' Expr ')'
Identifier ::=['a'-'z''_']+ ['a'-'z''_''0'-'9']*
String     ::=[^"]
Num    ::= [number][₽$]
```

## Compilation and usage

**TLDR**:

```bash
    git clone https://github.com/orientiered/MoneyLang.git && \
    cd MoneyLang && \
    git clone -b macroChaos https://github.com/orientiered/Processor.git && \
    make all -j4 && \
    cd Processor && \
    make all -j4 && \
    cd ..
```

0.Clone repository

```bash
    git clone https://github.com/orientiered/MoneyLang.git
    cd MoneyLang
    git clone -b macroChaos https://github.com/orientiered/Processor.git
```

1.Compile language tools: frontend and backend

```bash
    make all
```
2.Compile processor:

```bash
    cd Processor
    make all
    cd ..
```

3.Compile your program and run it

```bash
    ./run.sh yourProgram.mpp
```


## Frontend

```bash
    ./front.out program.mpp -o program.ast
```
High-level language code is parsed using recursive descent algorithm and converted to abstract syntax tree. Correspondence between operators and strings in AST is given by `IRNames` array in [Context.h](LangGlobals/include/Context.h).

## Reverse-frontend

```bash
    ./front.out program.ast -1 -o program.mpp
```
You could convert AST back to source code. Note: all comments will be lost. This tool can be used to convert languages with same AST or as auto-formatter.

## Middle-end

There's no middle-end which optimizes code, but you can use [crefr's](https://github.com/crefr/language) one, because IR is compatible.

## Backend

```bash
    ./back.out program.ast -o program.asm
    # add --taxes to get tax on every function return
```
AST is transformed to the Processor assembler. Language is focused on money, so you could add taxes with `--taxes` flag (20% by default).
