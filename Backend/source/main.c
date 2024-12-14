#include <stdlib.h>
#include <stdio.h>

#include "logger.h"
#include "utils.h"
#include "nameTable.h"
#include "backend.h"

int main() {
    logOpen("log.html", L_HTML_MODE);
    setLogLevel(L_EXTRA);
    logDisableBuffering();

    LangContext_t context = {0};
    BackendInit(&context, "../out.ast", "../compiled.asm", 1024, 128, 1024);

    BackendRun(&context);

    BackendDelete(&context);

    logClose();
    return 0;
}
