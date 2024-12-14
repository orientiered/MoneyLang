#include <stdlib.h>
#include <stdio.h>

#include "logger.h"
#include "utils.h"
#include "nameTable.h"
#include "frontend.h"

int main() {
    logOpen("log.html", L_HTML_MODE);
    setLogLevel(L_EXTRA);
    logDisableBuffering();

    LangContext_t context = {0};
    FrontendInit(&context, "test.mpp", "out.ast", 1024, 128, 1024, FRONTEND_FORWARD);

    programToTree(&context);

    FrontendDelete(&context);

    FrontendInit(&context, "out.ast", "revTest.mpp", 1024, 128, 1024, FRONTEND_BACKWARD);

    treeToProgram(&context);

    FrontendDelete(&context);

    logClose();
    return 0;
}
