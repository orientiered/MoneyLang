#ifndef NAMETABLE_H
#define NAMETABLE_H

const size_t NAMETABLE_BUFFER_SIZE = 128;
const int NULL_IDENTIFIER = -1;

enum IdentifierType {
    UNDEFINED_ID = 0,
    FUNC_ID,
    VAR_ID
};

typedef struct {
    char *str;
    enum IdentifierType type;
    bool local;
    size_t argsCount;
    size_t address;
} Identifier_t;

typedef struct {
    MemoryArena_t namesArray;

    Identifier_t *identifiers;
    size_t size;
    size_t capacity;
} NameTable_t;

typedef enum {
    NAMETABLE_SUCCESS,
    NAMETABLE_OVERFLOW,
    NAMETABLE_NAMES_OVERFLOW,
    NAMETABLE_WRONG_FILE_FORMAT
} NameTableStatus_t;

/*=====================NameTable functions==========================*/
NameTableStatus_t NameTableCtor(NameTable_t *table, size_t namesArrayCapacity, size_t capacity);

NameTableStatus_t NameTableDtor(NameTable_t *table);

NameTableStatus_t NameTableWrite(NameTable_t *table, FILE *file);

NameTableStatus_t NameTableRead(NameTable_t *table, const char **text);


int insertIdentifier(NameTable_t *table, const char *idName);

int findIdentifier(NameTable_t *table, const char *idName);

Identifier_t getIdFromTable(NameTable_t *table, size_t idx);

/*===================================================================*/


#endif
