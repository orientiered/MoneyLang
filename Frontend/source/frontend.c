#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "utils.h"
#include "logger.h"
#include "nameTable.h"
#include "frontend.h"

FrontendStatus_t FrontendInit(LangContext_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen,
                               enum FrontendMode_t mode)
{
    context->inputFileName = inputFileName;
    context->outputFileName = outputFileName;
    context->mode = mode;

    NameTableCtor(&context->nameTable, maxTotalNamesLen, maxNametableSize);
    if (mode == FRONTEND_FORWARD)
        context->treeMemory = createMemoryArena(maxTokens, sizeof(Token_t));
    else if (mode == FRONTEND_BACKWARD) {
        context->treeMemory = createMemoryArena(maxTokens, sizeof(Node_t));
    }

    context->text = readFileToStr(inputFileName);
    if (!context->text)
        return FRONTEND_FILE_ERROR;

    logPrint(L_EXTRA, 0, "Initialized frontend\n");
    return FRONTEND_SUCCESS;
}

FrontendStatus_t FrontendDelete(LangContext_t *context) {
    assert(context);

    free( (void *) context->text);
    freeMemoryArena(&context->treeMemory);

    NameTableDtor(&context->nameTable);
    memset(context, 0, sizeof(*context));
    logPrint(L_EXTRA, 0, "Deleted frontend\n");
    return FRONTEND_SUCCESS;
}

/*=========================Reverse frontend====================================*/
static bool needBrackets(Node_t *node) {
    assert(node);
    if (!node->parent)
        return false;

    if (node->type == IDENTIFIER)
        return false;

    if (node->type == NUMBER)
        return node->value.number < 0;

    if (node->type == OPERATOR) {
        if (!operators[node->value.op].binary)
            return !(node->value.op == OP_IN  || node->value.op == OP_OUT ||
                     node->value.op == OP_RET || node->value.op == OP_VAR_DECL);

        int currentPriority = operators[node->value.op].priority;
        int parentPriority  = operators[node->parent->value.op].priority;

        return (node->parent->value.op == OP_POW || parentPriority > currentPriority);
    }
    return false;
}

static void printTabs(FILE *file, unsigned tabs) {
    for (unsigned i = 0; i < tabs; i++) fputc('\t', file);
}

/// @brief Write scope in if, while, function declaration, etc.
static void writeScope(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs);

/// @brief Write sequence of operators that end with ;
static void writeSemicolon(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs);

static void writeAsProgramRecursive(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs);

static void writeIfWhileStatement(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->type == OPERATOR);
    assert(node->value.op == OP_IF || node->value.op == OP_WHILE);

    // printTabs(file, tabs);
    fprintf(file, "%s ", operators[node->value.op].str);
    writeAsProgramRecursive(context, file, node->left, tabs);
    fprintf(file, " %s \n", operators[OP_ARROW].str);

    if (node->right->type == OPERATOR && node->right->value.op == OP_ELSE) {
        writeScope(context, file, node->right->left, tabs+1);
        printTabs(file, tabs);
        fprintf(file, "%s \n", operators[OP_ELSE].str);
        writeScope(context, file, node->right->right, tabs+1);
    } else
        writeScope(context, file, node->right, tabs+1);
}

static void writeFunctionDecl(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->type == OPERATOR);
    assert(node->value.op == OP_FUNC_DECL);

    // printTabs(file, tabs);
    //transaction args
    fprintf(file, "%s ", operators[OP_FUNC_DECL].str);
    writeAsProgramRecursive(context, file, node->left->right, tabs);
    // -> name
    fprintf(file, " %s ", operators[OP_ARROW].str);
    writeAsProgramRecursive(context, file, node->left->left, tabs);
    // -> \n
    fprintf(file, " %s \n", operators[OP_ARROW].str);
    // body
    writeScope(context, file, node->right, tabs+1);
}

static void writeCall(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->type == OPERATOR);
    assert(node->value.op == OP_CALL);

    writeAsProgramRecursive(context, file, node->left, tabs);
    fprintf(file, "%s", operators[OP_LBRACKET].str);
    writeAsProgramRecursive(context, file, node->right, tabs);
    fprintf(file, "%s", operators[OP_RBRACKET].str);

}

static void writeComma(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->type == OPERATOR);
    assert(node->value.op == OP_COMMA);

    writeAsProgramRecursive(context, file, node->left, tabs);
    if (node->right) {
        fprintf(file, "%s ", operators[OP_COMMA].str);
        writeAsProgramRecursive(context, file, node->right, tabs);
    }
}

static void writeScope(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->type == OPERATOR);
    assert(node->value.op == OP_SEP);

    bool block = (node->right != NULL);

    if (block) {
        printTabs(file, tabs-1);
        fprintf(file, "<\n");
    }

    writeSemicolon(context, file, node, tabs);

    if (block) {
        printTabs(file, tabs-1);
        fprintf(file, ">\n");
    }
}

static void writeSemicolon(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->value.op == OP_SEP);

    printTabs(file, tabs);
    writeAsProgramRecursive(context, file, node->left, tabs);

    //TODO: get array of specific operators
    //! easy to forgot to add operator here
    if (node->left->value.op != OP_IF && node->left->value.op != OP_WHILE &&
        node->left->value.op != OP_FUNC_DECL && node->left->value.op != OP_SEP)
        fprintf(file, " %s", operators[OP_SEP].str);
    fprintf(file, "\n");

    writeAsProgramRecursive(context, file, node->right, tabs);
}

static void writeAsProgramRecursive(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    if (!node) return;

    if (node->type == NUMBER) {
        fprintf(file, "%lg₽", node->value.number);
        return;
    } else if (node->type == IDENTIFIER) {
        fprintf(file, "%s", getIdFromTable(&context->nameTable, node->value.id).str);
        return;
    }

    if (node->value.op == OP_IF || node->value.op == OP_WHILE) {
        writeIfWhileStatement(context, file, node, tabs);
        return;
    } else if (node->value.op == OP_FUNC_DECL) {
        writeFunctionDecl(context, file, node, tabs);
        return;
    } else if (node->value.op == OP_CALL) {
        writeCall(context, file, node, tabs);
        return;
    } else if (node->value.op == OP_SEP) {
        writeSemicolon(context, file, node, tabs);
        return;
    } else if (node->value.op == OP_COMMA) {
        writeComma(context, file, node, tabs);
        return;
    }

    bool binary = operators[node->value.op].binary;
    bool brackets = needBrackets(node);

    if (!binary) {
        fprintf(file, "%s", operators[node->value.op].str);
        fprintf(file, (brackets) ? "(" : " ");

        writeAsProgramRecursive(context, file, node->left, tabs);

        fprintf(file, (brackets) ? ")" : " ");

    } else {

        if (brackets)
            fprintf(file, "(");

        writeAsProgramRecursive(context, file, node->left, tabs);
        fprintf(file, " %s ", operators[node->value.op].str);
        writeAsProgramRecursive(context, file, node->right, tabs);

        if (brackets)
            fprintf(file, ")");

    }
}

FrontendStatus_t writeAsProgram(LangContext_t *context) {
    assert(context);
    assert(context->tree);
    assert(context->outputFileName);

    FILE *file = fopen(context->outputFileName, "w");
    if (!file) {
        logPrint(L_ZERO, 1, "Can't open file '%s' for writing\n", context->outputFileName);
        return FRONTEND_FILE_ERROR;
    }

    fprintf(file, "%c Autogenerated\n", COMMENT_START_SYMBOL);
    writeAsProgramRecursive(context, file, context->tree, 0);

    fclose(file);

    return FRONTEND_SUCCESS;
}


FrontendStatus_t programToTree(LangContext_t *context) {
    FrontendStatus_t status = lexicalAnalysis(context);
    if (status != FRONTEND_SUCCESS)
        return status;

    status = lexerDump(context);
    if (status != FRONTEND_SUCCESS)
        return status;

    status = syntaxAnalysis(context);
    if (status != FRONTEND_SUCCESS)
        return status;
    DUMP_TREE(context, context->tree, 0);

    ASTStatus_t astStatus = writeAsAST(context);
    if (astStatus != AST_SUCCESS)
        return FRONTEND_AST_ERROR;

    return FRONTEND_SUCCESS;
}

FrontendStatus_t treeToProgram(LangContext_t *context) {
    ASTStatus_t astStatus = readFromAST(context);
    if (astStatus != AST_SUCCESS)
        return FRONTEND_AST_ERROR;

    DUMP_TREE(context, context->tree, 0);

    FrontendStatus_t status = writeAsProgram(context);
    if (status != FRONTEND_SUCCESS)
        return status;

    return FRONTEND_SUCCESS;
}

FrontendStatus_t frontendRun(LangContext_t *context) {
    assert(context);
    if (context->mode == FRONTEND_FORWARD)
        return programToTree(context);
    else
        return treeToProgram(context);
}
