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
            return !(node->value.op == OP_IN || node->value.op == OP_OUT);

        int currentPriority = operators[node->value.op].priority;
        int parentPriority  = operators[node->parent->value.op].priority;

        return (node->parent->value.op == OP_POW || parentPriority > currentPriority);
    }
    return false;
}

static void printTabs(FILE *file, unsigned tabs) {
    for (unsigned i = 0; i < tabs; i++) fputc('\t', file);
}

static void writeAsProgramRecursive(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs);

static void writeIfStatement(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->type == OPERATOR);
    assert(node->value.op == OP_IF);

    printTabs(file, tabs);
    fprintf(file, "%s ", operators[node->value.op].str);
    writeAsProgramRecursive(context, file, node->left, tabs);
    fprintf(file, " %s \n", operators[OP_ARROW].str);
    printTabs(file, tabs);
    writeAsProgramRecursive(context, file, node->right, tabs+1);
}

static void writeSemicolon(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    assert(node->value.op == OP_SEMICOLON);

    bool block = node->left->value.op == OP_SEMICOLON;
    printTabs(file, tabs);
    if (block) {
        fprintf(file, "<\n");
    }
    writeAsProgramRecursive(context, file, node->left, tabs + block);
    if (block) {
        printTabs(file, tabs);
        fprintf(file, ">");
    }

    if (node->left->value.op != OP_IF && node->left->value.op != OP_SEMICOLON)
        fprintf(file, " %s", operators[OP_SEMICOLON].str);
    fprintf(file, "\n");

    writeAsProgramRecursive(context, file, node->right, tabs);
}

static void writeAsProgramRecursive(LangContext_t *context, FILE *file, Node_t *node, unsigned tabs) {
    if (!node) return;

    if (node->type == NUMBER) {
        fprintf(file, "%lg₽", node->value.number);
        return;
    } else if (node->type == IDENTIFIER) {
        fprintf(file, "%s", getIdentifier(&context->nameTable, node->value.id).str);
        return;
    }

    if (node->value.op == OP_IF) {
        writeIfStatement(context, file, node, tabs);
        return;
    } else if (node->value.op == OP_SEMICOLON) {
        writeSemicolon(context, file, node, tabs);
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

    IRStatus_t irStatus = writeAsIR(context);
    if (irStatus != IR_SUCCESS)
        return FRONTEND_IR_ERROR;

    return FRONTEND_SUCCESS;
}

FrontendStatus_t treeToProgram(LangContext_t *context) {
    IRStatus_t irStatus = readFromIR(context);
    if (irStatus != IR_SUCCESS)
        return FRONTEND_IR_ERROR;

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
