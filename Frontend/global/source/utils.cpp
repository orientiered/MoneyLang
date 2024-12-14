#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "utils.h"

long long maxINT(long long a, long long b) {
    return (a > b) ? a : b;
}

long long minINT(long long a, long long b) {
    return (a > b) ? b : a;
}

int myStricmp(const char *strA, const char *strB) {
//TODO: maybe in far far future optimize this function
    while (*strA && (tolower(*strA) == tolower(*strB)) ) strA++, strB++;
    return tolower(*strA) -  tolower(*strB);
}

void percentageBar(size_t value, size_t maxValue, unsigned points, long long timePassed) {
    //draw nice progress bar like
    //[###|-----] 20.0% Remaining time: 20.4 s
    printf("\r[");
    for (unsigned i = 0; i < points; i++) {
        double pointFill = double(value) / maxValue - double(i) / points;
        if (pointFill > 0)
            printf("#");
        else if (pointFill > -0.5 / points)
            printf("|");
        else
            printf("-");
    }
    printf("] %5.1f%%", double(value)/maxValue * 100);
    if (value > 0 && timePassed > 0) {
        printf(" Remaining time: %4.1f s", double(timePassed) / value * (maxValue - value) / CLOCKS_PER_SEC);
    }
    fflush(stdout);
}

void swap(void* a, void* b, size_t len) {
    //checking if a and b are correctly aligned
    const unsigned blockSize = sizeof(uint64_t);
    if ((((size_t) a) % blockSize != (((size_t) b) % blockSize))) { //TODO: sizeof(long long/uint64_t)
        swapByByte(a, b, len);
        return;
    }
    //aligning a and b if possible
    const unsigned startOffset = (blockSize - ((size_t) a % blockSize)) % blockSize;
    swapByByte(a, b, startOffset);

    size_t llSteps = (len-startOffset) / blockSize;
    //swapping every 8 bytes
    uint64_t *lla = (uint64_t*) ((size_t)a + startOffset), *llb = (uint64_t*) ((size_t)b + startOffset);
    uint64_t temp = 0;

    while (llSteps--) {
        temp = *lla;
        *lla++ = *llb;
        *llb++ = temp;
    }

    swapByByte(lla, llb, (len-startOffset) % blockSize);
}

void swapByByte(void* a, void* b, size_t len) {
    char *ac = (char*) a, *bc = (char*) b;
    char c = 0;
    while (len--) {
        c = *ac;
        *ac++ = *bc;
        *bc++ = c;
    }
}

void reverseArray(void *array, size_t elemSize, size_t len) {
    char *left  = (char *)array,
         *right = (char *)array + elemSize * (len-1);
    while (left < right) {
        swap(left, right, elemSize);
        left += elemSize; right -= elemSize;
    }
}

doublePair_t runningSTD(double value, int getResult) {
    //function to calculate standard deviation of some value
    //constructed to make calculations online, so static variables
    static doublePair_t result = {};
    static unsigned measureCnt = 0;     //number of values
    static double totalValue = 0;       //sum of value
    static double totalSqrValue = 0;    //sum of value^2
    // getResult > 1 --> calculate meanValue and std and return it
    // getResult = 0 --> store current value
    // getResult < 0 --> reset stored values
    if (getResult > 0) {
        if (measureCnt > 1) {
            result.first = totalValue / measureCnt;
            //printf("%g %g %u\n", totalSqrValue, totalValue, measureCnt);
            result.second = sqrt(totalSqrValue / measureCnt - result.first*result.first) / sqrt(measureCnt - 1);
        }
        return result;
    } else if (getResult == 0) {
        measureCnt++;
        totalValue += value;
        totalSqrValue += value*value;
    } else {
        measureCnt = 0;
        totalValue = totalSqrValue = 0;
    }
    return result;
}

void memValSet(void *start, const void *elem, size_t elemSize, size_t length) {
    char *ptr = (char*) start;
    const char *elemPtr = (const char*) elem;
    while (length--) {
        memcpy(ptr, elemPtr, elemSize);
        ptr += elemSize;
    }
}

// DJB2 hash //link
uint64_t memHash(const void *arr, size_t len) {
    if (!arr) return 0x1DED0BEDBAD0C0DE;
    uint64_t hash = 5381;
    const unsigned char *carr = (const unsigned char*)arr;
    while (len--)
        hash = ((hash << 5) + hash) + *carr++;
        //hash = 33*hash + c
    return hash;
}

/*==========================================================================*/

MemoryArena_t createMemoryArena(size_t capacity, size_t elemSize) {
    MemoryArena_t arena = {.elemSize = elemSize, .capacity = capacity};
    arena.base = calloc(capacity, elemSize);
    arena.current = arena.base;
    return arena;
}

void *getMemory(MemoryArena_t *arena) {
    assert(arena);
    return getMemoryS(arena, arena->elemSize);
}

void *getMemoryS(MemoryArena_t *arena, size_t elemSize) {
    assert(arena);
    if ((char *)arena->current + elemSize >= (char *)arena->base + arena->capacity * arena->elemSize) {
        fprintf(stderr, "Not enough memory in arena[%p]:\ncapacity = %zu, need = %zu\n",
                        arena, arena->capacity, (size_t) ((char *)arena->current + elemSize - (char *)arena->base) );
        return NULL;
    }

    void *mem = arena->current;
    arena->current = (char *) arena->current + elemSize;

    return mem;
}

int freeMemoryArena(MemoryArena_t *arena) {
    free(arena->base);
    arena->base = NULL;
    arena->current = NULL;
    return 0;
}

/*==================================================================================*/

/// @brief Read file to allocated buffer
/// @param fileName
/// @return File content
char *readFileToStr(const char *fileName) {
    FILE *file = fopen(fileName, "r");
    if (!file) {
        fprintf(stderr, "Failed to open file %s\n", fileName);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *text = (char *) calloc(fileSize+1, sizeof(char));
    if (fread(text, sizeof(char), fileSize, file) != fileSize) {
        free(text);
        fprintf(stderr, "Failed to read file %s\n", fileName);
        return NULL;
    }

    return text;
}
