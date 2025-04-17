#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "logger.h"
#include "utils.h"

#include "nameTable.h"
#include "frontend.h"

static void skipSpaces(const char **str, size_t *line, size_t *col) {
    // logPrint(L_EXTRA, 0, "Skipping comments starting from %10s\n", *str);

    bool inComment = false;
    while (1) {
        if (**str == COMMENT_START_SYMBOL)
            inComment = true;

        if (!inComment && !isspace(**str))
            break;

        if (**str == '\n') {
            inComment = false;
            (*line)++;
            *col = 1;
        } else
            (*col)++;

        (*str)++;
    }
    // logPrint(L_EXTRA, 0, "Skipped comments: %10s\n", *str);
}

static size_t readLexemToBuffer(char *buffer, const char **str, size_t *col, bool previousIsQuote) {
    size_t len = 0;
    //[a-z_] -> [a-z0-9_]*
    //[any symbol that is not alpha, num, space, COMMENT_START_SYMBOL, ()]+
    //TODO: rewrite this code, too ambiguous
    if (previousIsQuote) {
        while (**str && **str != '\"') {
            *(buffer++) = *((*str)++);
            (*col)++;
            len++;
        }
    } else if (isalpha(**str) || (**str == '_')) {
        while (isalnum(**str) || **str == '_') {
            *(buffer++) = *((*str)++);
            (*col)++;
            len++;
        }
    } else if ((**str) == '(' || (**str) == ')') {
        *(buffer++) = *((*str)++);
        (*col)++;
        len++;
    } else {
        while (!isalnum(**str) && !isspace(**str) &&
             (**str) != '(' && (**str) != ')' &&
             (**str) != COMMENT_START_SYMBOL) {
            *(buffer++) = *((*str)++);
            (*col)++;
            len++;
        }
    }

    return len;
}

/// @brief find longest operator from given position.
/// @param [out] len Length of found operator
/// @return idx of operator if found, -1 otherwise
static int findOperator(const char *text, size_t *len) {
    assert(text);
    assert(len);

    int result = -1;
    size_t longestMatch = 0;

    for (unsigned idx = 0; idx < ARRAY_SIZE(operators); idx++) {
        if (operators[idx].str == NULL) continue;
        size_t operatorLen = strlen(operators[idx].str);
        if (strncmp(operators[idx].str, text, operatorLen) == 0)
            if (operatorLen > longestMatch) {
                longestMatch = operatorLen;
                result = idx;
            }
    }

    *len = longestMatch;
    return result;
}

/// @brief Tokenize text
/// @param context
/// @return status
FrontendStatus_t lexicalAnalysis(LangContext_t *context) {
    assert(context);

    logPrint(L_EXTRA, 0, "Starting tokenization\n");
    if (context->mode != FRONTEND_FORWARD)
        return FRONTEND_WRONG_MODE_ERROR;

    assert(context->treeMemory.capacity);

    const char *curStr = context->text;
    size_t curLine = 1, curCol = 1;

    Token_t *tokens = (Token_t *) context->treeMemory.base;
    size_t tokenIdx = 0;
    size_t capacity = context->treeMemory.capacity;

    #define token (tokens[tokenIdx])

    #define nextToken \
        do {                                                                                    \
            if (tokenIdx < capacity - 1)                                                        \
                tokenIdx++;                                                                     \
            else {                                                                              \
                logPrint(L_ZERO, 1, "Reached limit of tokens: %d. Please increase limit\n",     \
                         capacity);                                                             \
                return FRONTEND_MEMORY_ERROR;                                                   \
            }                                                                                   \
        } while (0)

    #define lexerError(...) \
        do {                                                                                            \
            logPrint(L_ZERO, 1, "Lexer error at %s:%s:%d\n", context->inputFileName, curLine, curCol);  \
            logPrint(L_ZERO, 1, __VA_ARGS__);                                                           \
            return FRONTEND_LEXER_ERROR;                                                                \
        } while (0)


    skipSpaces(&curStr, &curLine, &curCol);
    while (*curStr) {

        //filling position of token
        token.line   = curLine;
        token.column = curCol;
        token.pos    = curStr;
        // all numbers must start with digit
        if (isdigit(*curStr)) {
            char *nextPosition = NULL;
            double number = strtod(curStr, &nextPosition);
            if (nextPosition > curStr) {
                logPrint(L_EXTRA, 0, "LEXER: Adding number: %lg\n", number);
                token.node.type = NUMBER;
                token.node.value.number = number;
                curCol += nextPosition - curStr;
                curStr = nextPosition;
            } else
                lexerError("Can't parse number\n");

        } else {
            // trying to find operator
            size_t lexemLen = 0;
            int idx = findOperator(curStr, &lexemLen);
            if (idx >= 0) {
                logPrint(L_EXTRA, 0, "LEXER: Adding operator %d (%s)\n", idx, operators[idx].str);
                curStr += lexemLen;
                curCol += lexemLen;
                token.node.type = OPERATOR;
                token.node.value.op = (enum OperatorType)idx;
            } else {
                char buffer[LEXER_BUFFER_SIZE] = "";
                bool previousIsQuote = false;
                if (tokenIdx > 0) previousIsQuote = (tokens[tokenIdx-1].node.type == OPERATOR && tokens[tokenIdx-1].node.value.op == OP_QUOTE);
                lexemLen = readLexemToBuffer(buffer, &curStr, &curCol, previousIsQuote);

                if (lexemLen == 0)
                    lexerError("Can't read lexem\n");

                logPrint(L_EXTRA, 0, "LEXER: Adding identifier: '%s'\n", buffer);
                idx = insertIdentifier(&context->nameTable, buffer);
                token.node.type = IDENTIFIER;
                token.node.value.id = idx;
            }
        }

        nextToken;
        skipSpaces(&curStr, &curLine, &curCol);

    }

    token.line   = curLine;
    token.column = curCol;
    token.pos    = curStr;

    token.node.type = OPERATOR;
    token.node.value.op = OP_EOF;

    //Telling MemoryArena how much memory was used
    context->treeMemory.current = tokens + tokenIdx + 1;
    return FRONTEND_SUCCESS;
}

#undef token
#undef lexerError
#undef nextToken

FrontendStatus_t lexerDump(LangContext_t *context) {
    logPrint(L_ZERO, 0, "LEXER_DUMP\n");
    if (!context) {
        logPrint(L_ZERO, 1, "ERROR: null pointer passed\n");
        return FRONTEND_LEXER_ERROR;
    }

    if (context->mode != FRONTEND_FORWARD) {
        logPrint(L_ZERO, 1, "Dump called with wrong frontend mode\n");
        return FRONTEND_LEXER_ERROR;
    }

    Token_t *tokens = (Token_t *)context->treeMemory.base;
    size_t capacity = context->treeMemory.capacity;

    for (size_t idx = 0; idx < capacity; idx++) {
        Token_t *current = tokens + idx;
        logPrint(L_ZERO, 0, "TOKEN#%03d:\n", idx);
        logPrint(L_ZERO, 0, "\tPos: %d:%d\n", current->line, current->column);
        switch(current->node.type) {
            case OPERATOR:
                logPrint(L_ZERO, 0, "\tOperator: '%s' (%d)\n",
                        operators[current->node.value.op].str,
                        current->node.value.op);

                if (current->node.value.op == OP_EOF) {
                    logPrint(L_ZERO, 0, "LEXER: found EOF\nLEXER DUMP END\n");
                    return FRONTEND_SUCCESS;
                }

                break;
            case NUMBER:
                logPrint(L_ZERO, 0, "\tNumber: %lg\n", current->node.value.number);
                break;
            case IDENTIFIER:
                logPrint(L_ZERO, 0, "\tIdentifier: '%s' (%d)\n",
                getIdFromTable(&context->nameTable, current->node.value.id).str, current->node.value.id);
                break;
            default:
                assert(0);

        }
    }

    logPrint(L_ZERO, 0, "LEXER DUMP: ERROR: No EOF\n");
    return FRONTEND_LEXER_ERROR;
}
