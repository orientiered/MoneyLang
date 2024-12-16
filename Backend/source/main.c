#include <stdlib.h>
#include <stdio.h>

#include "logger.h"
#include "argvProcessor.h"
#include "utils.h"
#include "nameTable.h"
#include "backend.h"


const int ARGV_EXIT_CODE = 3;
const int NO_FILE_EXIT_CODE = 4;

const size_t DEFAULT_MAX_TOKENS     = 1024;
const size_t DEFAULT_NAMETABLE_SIZE = 256;
const size_t DEFAULT_NAMES_LEN      = 2048;

int main(int argc, const char *argv[]) {
    logOpen("log.html", L_HTML_MODE);
    setLogLevel(L_EXTRA);
    logDisableBuffering();

    registerFlag(TYPE_STRING, "-i", "--input",  "Input file");
    registerFlag(TYPE_STRING, "-o", "--output", "Output file");

    registerFlag(TYPE_INT,    "-t", "--tokens", "Maximum number of tokens");
    registerFlag(TYPE_INT,    "-n", "--nameTableSize", "Maximum number of records in nametable");
    registerFlag(TYPE_INT,    "-l", "--namesLen", "Maximum total length of all names in nametable");

    enableHelpFlag("Money language backend: transform IR files to SPU asm\n");

    if (processArgs(argc, argv) != ARGV_SUCCESS) {
        return ARGV_EXIT_CODE;
    }

    const char *inputFileName = getDefaultArgument(0);
    if (!inputFileName) inputFileName = getFlagValue("-i").string_;
    if (!inputFileName) {
        logPrint(L_ZERO, 1, "No input file specified\n");
        return NO_FILE_EXIT_CODE;
    }

    const char *outputFileName = getFlagValue("-o").string_;
    if (!outputFileName) {
        logPrint(L_ZERO, 1, "No output file specified\n");
        return NO_FILE_EXIT_CODE;
    }

    size_t maxTokens = getFlagValue("-t").int_;
    if (maxTokens == 0) maxTokens = DEFAULT_MAX_TOKENS;

    size_t nameTableSize = getFlagValue("-n").int_;
    if (nameTableSize == 0) nameTableSize = DEFAULT_NAMETABLE_SIZE;

    size_t namesLen = getFlagValue("-l").int_;
    if (namesLen == 0) namesLen = DEFAULT_NAMES_LEN;

    LangContext_t context = {0};
    BackendInit(&context, inputFileName, outputFileName, maxTokens, nameTableSize, namesLen);

    BackendRun(&context);

    BackendDelete(&context);

    logClose();
    return 0;
}
