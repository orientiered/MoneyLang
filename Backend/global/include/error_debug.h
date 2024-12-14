/// @file
/// @brief Exit status, error handling

#ifndef ERROR_DEBUG_H
#define ERROR_DEBUG_H

#include "errno.h"
#include "colors.h"


/*!
    @brief Propagate error and run custom instructions before returns

*/
#define USER_ERROR(expr, ...)           \
    do {                                \
        enum status res = (expr);       \
        if (res != SUCCESS) {           \
            { __VA_ARGS__; }            \
            return res;                 \
        }                               \
    } while(0)


/*!
    @brief Propagate error

    @param[in] expr Expression of type enum status

    If expr is ERROR or FAIL, macro will print file, function and line where expression ocurred <br>
    Then it will return expr <br>
    Prints to stderr
*/
#define PROPAGATE_ERROR(expr)                                                                                               \
        do{                                                                                                                 \
            enum status res = (expr);                                                                                         \
            if (res != SUCCESS)                                                                                             \
            {                                                                                                               \
                /*fprintf(stderr, "Error. File: %s, function: %s, line: %d\n", __FILE__, __FUNCTION__, __LINE__);*/         \
                return res;                                                                                                 \
            }                                                                                                               \
        }while(0)

/*!
    @brief Assert with custom behaviour

    @param[in] expr Any expression of integer type
    @param[in] ... Set of commands to run

    If expr is false (=0), macro will print file, function and line where assertion was made <br>
    ALso prints expression to stderr <br>
    Will run all commands after expression <br>
    You can deactivate assert by defining NDEBUG
*/


#ifndef NDEBUG
#define MY_ASSERT(expr, ...)                                                                                                    \
        do {                                                                                                                    \
            if (!(expr)) {                                                                                                      \
                fprintf(stderr, RED "Assertion failed:\n\t[" #expr "]\n" RESET_C);                                              \
                fprintf(stderr, RED "%s:%d, function: %s\n" RESET_C, __FILE__, __LINE__, __PRETTY_FUNCTION__);                  \
                {                                                                                                               \
                    __VA_ARGS__;                                                                                                \
                }                                                                                                               \
            }                                                                                                                   \
        }while(0)
#else
#define MY_ASSERT(expr, run)
#endif


/*!
    @brief Printf for debug

    Activated by defining DEBUG_PRINTS, defines DBG_PRINTF(...) macro
*/
//#define DEBUG_PRINTS
#ifdef DEBUG_PRINTS
# ifndef NDEBUG
#define DBG_PRINTF(...)                     \
    do {                                    \
        fprintf(stderr, __VA_ARGS__);       \
    } while (0)
# else
#define DBG_PRINTF(...)
# endif
#else
#define DBG_PRINTF(...)
#endif

#ifdef DEBUG_PRINTS
# ifndef NDEBUG
#define DBG_STR(str, size)                                      \
    do {                                                        \
        for (int i = 0;str[i] && i < size; i++) {               \
            printf("%c(%d)", str[i], (unsigned char)str[i]);    \
        }                                                       \
    } while (0)
# else
#define DBG_STR(str, size)
# endif
#else
#define DBG_STR(str, size)
#endif
/// @brief Error codes which can be used in many functions
enum status {
    SUCCESS = 0,        ///< Success
    ERROR,              ///< Some type of error occurred
    LOGIC_ERROR,        ///< Return from wrong place in function
    EMPTY_STATUS        ///< Empty error
};

#endif
