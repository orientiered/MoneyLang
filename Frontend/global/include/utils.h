/// @file
/// @brief
#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

#define CALLOC(elemNumber, Type) (Type *) calloc(elemNumber, sizeof(Type))

#define FREE(ptr) do {free(ptr); ptr = NULL;} while (0)

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))

/*------------------STRUCTS DEFINITIONS---------------------------------------*/

typedef struct doublePair {
    double first;
    double second;
} doublePair_t;

typedef struct llPair {
    long long first;
    long long second;
} llPair_t;

typedef struct ullPair {
    unsigned long long first;
    unsigned long long second;
} ullPair_t;

typedef struct voidPtrPair {
    void *first;
    void *second;
} voidPtrPair_t;

/*==================MEMORY ARENA - STACK BASED ALLOCATOR======================*/

typedef struct MemoryArena {
    void *base;
    void *current;
    size_t elemSize;
    size_t capacity;
} MemoryArena_t;

MemoryArena_t createMemoryArena(size_t capacity, size_t elemSize);

void *getMemory(MemoryArena_t *arena);
void *getMemoryS(MemoryArena_t *arena, size_t elemSize);

int freeMemoryArena(MemoryArena_t *arena);

#define GET_MEMORY(arena, Type) (Type *) getMemory(arena);
#define GET_MEMORY_S(arena, size, Type) (Type *) getMemoryS(arena, size);

/*------------------SIMPLE AND CONVENIENT FUNCTIONS---------------------------*/

long long maxINT(long long a, long long b);
long long minINT(long long a, long long b);

/// @brief compare strings ignoring case
int myStricmp(const char *strA, const char *strB);

///time passed in ms
void percentageBar(size_t value, size_t maxValue, unsigned points, long long timePassed);

void swap(void* a, void* b, size_t len);
void swapByByte(void* a, void* b, size_t len);

void reverseArray(void *array, size_t elemSize, size_t len);

/// @brief memset with multiple byte values
void memValSet(void *start, const void *elem, size_t elemSize, size_t length);

/// @brief Incrementally compute standard deviation
/// @return Pair with mean value and its delta
/// getResult > 1 --> calculate meanValue and std and return it <br>
/// getResult = 0 --> store current value <br>
/// getResult < 0 --> reset stored values <br>
doublePair_t runningSTD(double value, int getResult);

/// @brief djb2 hash for any data
uint64_t memHash(const void *arr, size_t len);

/// @brief Read file to allocated buffer
/// @param fileName
/// @return File content
char *readFileToStr(const char *fileName);

#endif
