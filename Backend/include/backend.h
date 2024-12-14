#ifndef BACKEND_H
#define BACKEND_H

#include "Context.h"

typedef enum BackendStatus_t {
    BACKEND_SUCCESS,
    BACKEND_IR_ERROR,
    BACKEND_FILE_ERROR,
    BACKEND_WRITE_ERROR
} BackendStatus_t;

/// @brief Initialize frontend context
BackendStatus_t BackendInit(LangContext_t *context, const char *inputFileName, const char *outputFileName,
                               size_t maxTokens, size_t maxNametableSize, size_t maxTotalNamesLen);
/// @brief Delete frontend context
BackendStatus_t BackendDelete(LangContext_t *context);

BackendStatus_t BackendRun(LangContext_t *context);

BackendStatus_t translateToAsm(LangContext_t *context);
#endif
