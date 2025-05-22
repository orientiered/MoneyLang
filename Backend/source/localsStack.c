#include <stdio.h>
#include <stdlib.h>

#include "logger.h"
#include "backend.h"

/* =============== Implementation independent functions ====================== */

LocalVar_t *LocalsStackTop(LocalsStack_t *stk) {
    return (stk->size > 0) ? stk->vars + stk->size - 1 : NULL;
}

BackendStatus_t LocalsStackInit(LocalsStack_t *stk, size_t capacity) {
    stk->capacity = capacity;
    stk->vars = CALLOC(capacity, LocalVar_t);
    stk->size = 0;
    return BACKEND_SUCCESS;
}

BackendStatus_t LocalsStackDelete(LocalsStack_t *stk) {
    free(stk->vars);
    return BACKEND_SUCCESS;
}

BackendStatus_t LocalsStackPopScope(LocalsStack_t *stk, size_t *variables) {
    logPrint(L_EXTRA, 0, "Popping scope\n");

    size_t poppedVariables = 0;

    while (stk->size && LocalsStackTop(stk)->id >= 0) {
        stk->size--;
        poppedVariables++;
    }

    //stopped on scope separator
    if (stk->size) stk->size--;

    if (variables) *variables = poppedVariables;

    return BACKEND_SUCCESS;
}

BackendStatus_t LocalsStackInitScope(LocalsStack_t *stk, enum ScopeType scope) {
    logPrint(L_EXTRA, 0, "Creating new scope %d\n", scope);
    stk->vars[stk->size].id = scope;
    //if it is normal scope and we have elements before, address numeration continues
    if (scope == NORMAL_SCOPE && stk->size > 0) {
        stk->vars[stk->size].address = stk->vars[stk->size-1].address;
    }

    stk->size++;

    return BACKEND_SUCCESS;
}
