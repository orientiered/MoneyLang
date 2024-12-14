#ifndef FRONTEND_H
#define FRONTEND_H

#include "Context.h"

const size_t PREFIX_PARSER_BUFFER_SIZE = 32;
const size_t LEXER_BUFFER_SIZE = 128;


const char COMMENT_START_SYMBOL = '@';
const double DOUBLE_EPSILON = 1e-12; //epsilon for comparing doubles

const double DOLLAR_TO_RUBLE = 35.0;

typedef enum FrontendStatus_t {
    FRONTEND_SUCCESS,
    FRONTEND_LEXER_ERROR,
    FRONTEND_SYNTAX_ERROR,
    FRONTEND_MEMORY_ERROR,
    FRONTEND_FILE_ERROR,
    FRONTEND_DUMP_ERROR,
    FRONTEND_IR_ERROR,
    FRONTEND_WRONG_FILE_FORMAT,
    FRONTEND_WRONG_MODE_ERROR
} FrontendStatus_t;

typedef enum FrontendMode_t {
    FRONTEND_FORWARD,           ///< parse program to tree
    FRONTEND_BACKWARD           ///< convert tree to program
} FrontendMode_t;


/*==========================Frontend context==========================*/
/// @brief Initialize frontend context
FrontendStatus_t FrontendInit(LangContext_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen,
                               enum FrontendMode_t mode);
/// @brief Delete frontend context
FrontendStatus_t FrontendDelete(LangContext_t *context);

/*=========================Creating expressions from strings===================*/
FrontendStatus_t frontendRun(LangContext_t *context);

FrontendStatus_t programToTree(LangContext_t *context);
FrontendStatus_t treeToProgram(LangContext_t *context);

FrontendStatus_t lexicalAnalysis(LangContext_t *context);
FrontendStatus_t syntaxAnalysis(LangContext_t *context);

FrontendStatus_t writeAsProgram(LangContext_t *context);

/*================================Dump tools==============================*/

FrontendStatus_t lexerDump(LangContext_t *context);

#endif
