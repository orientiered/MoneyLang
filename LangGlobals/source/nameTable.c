#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "logger.h"
#include "nameTable.h"

NameTableStatus_t NameTableCtor(NameTable_t *table, size_t namesArrayCapacity, size_t capacity) {
    assert(table);

    table->capacity = capacity;
    table->size = 0;

    table->namesArray = createMemoryArena(namesArrayCapacity, 1);

    table->identifiers = (Identifier_t *) calloc(capacity, sizeof(Identifier_t));

    return NAMETABLE_SUCCESS;
}

NameTableStatus_t NameTableDtor(NameTable_t *table) {
    assert(table);

    free(table->identifiers);
    freeMemoryArena(&table->namesArray);

    return NAMETABLE_SUCCESS;
}

int insertIdentifier(NameTable_t *table, const char *idName) {
    assert(idName);

    logPrint(L_EXTRA, 0, "Inserting identifier '%s'\nCurrent length = %d\n", idName, table->size);

    for (size_t idx = 0; idx < table->size; idx++) {
        if (strcmp(table->identifiers[idx].str, idName) == 0) {
            logPrint(L_EXTRA, 0, "\tAlready exists at idx=%d\n", idx);
            return idx;
        }
    }

    if (table->size == table->capacity) {
        logPrint(L_ZERO, 1, "ERROR:!!! NameTable overflow !!!\n");
        return NULL_IDENTIFIER;
    }

    size_t idLen = strlen(idName);

    char *idStr = GET_MEMORY_S(&table->namesArray, idLen + 1, char);
    if (!idStr)
        return NULL_IDENTIFIER;

    table->identifiers[table->size].str = idStr;
    table->identifiers[table->size].len = idLen;
    table->identifiers[table->size].type = UNDEFINED_ID;
    strcpy(idStr, idName);

    return table->size++;
}

int findIdentifier(NameTable_t *table, const char *idName) {
    assert(idName);

    for (size_t idx = 0; idx < table->size; idx++) {
        if (strcmp(table->identifiers[idx].str, idName) == 0)
            return idx;
    }

    return NULL_IDENTIFIER;
}

Identifier_t getIdFromTable(NameTable_t *table, size_t idx) {
    assert(table);
    assert(idx < table->size);

    return table->identifiers[idx];
}

NameTableStatus_t NameTableWrite(NameTable_t *table, FILE *file) {
    fprintf(file, "NAMETABLE\nsize:%d {\n", table->size);
    for (size_t idx = 0; idx < table->size; idx++) {
        // const char *type =
        fprintf(file, "\t\"%s\" %d\n", table->identifiers[idx].str, table->identifiers[idx].type);
    }
    fprintf(file, "}\n");
    return NAMETABLE_SUCCESS;
}

NameTableStatus_t NameTableRead(NameTable_t *table, const char **text) {
    size_t size = 0;
    int shift = 0;
    if (sscanf(*text, " NAMETABLE size:%d {%n", &size, &shift) != 1) {
        logPrint(L_ZERO, 1, "Wrong nametable format\n");
        return NAMETABLE_WRONG_FILE_FORMAT;
    }

    *text += shift;

    if (size > table->capacity) {
        logPrint(L_ZERO, 1, "Nametable overflow:\n"
                            "Capacity: %d\n"
                            "Need:     %d\n", table->capacity, size);

        return NAMETABLE_OVERFLOW;
    }

    table->size = size;

    char buffer[NAMETABLE_BUFFER_SIZE] = "";
    size_t nameLen = 0;

    for (size_t idx = 0; idx < table->size; idx++) {
        size_t start = 0, end = 0;
        if (sscanf(*text, " \"%n%[^\"]\"%n", &start, buffer, &end) != 1) {
            logPrint(L_ZERO, 1, "Wrong nametable format\n");
            return NAMETABLE_WRONG_FILE_FORMAT;
        }

        nameLen = end - start;
        *text += end;

        char *idStr = GET_MEMORY_S(&table->namesArray, nameLen + 1, char);
        if (!idStr) {
            logPrint(L_ZERO, 1, "ERROR:!!! NameTable namesArray overflow !!!\n");
            return NAMETABLE_NAMES_OVERFLOW;
        }

        strcpy(idStr, buffer);
        table->identifiers[idx].str = idStr;

        if (sscanf(*text, " %d%n", &table->identifiers[idx].type, &shift) != 1) {
            logPrint(L_ZERO, 1, "Wrong nametable format\n");
            return NAMETABLE_WRONG_FILE_FORMAT;
        }

        *text += shift;
    }

    sscanf(*text, " }%n", &shift);

    *text += shift;
    return NAMETABLE_SUCCESS;
}
