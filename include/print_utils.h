#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#include <stdio.h>
#include <stdbool.h>

extern bool gDebugParam;

/* Printing */
/**< Prints the name of the function and the arguments only if gDebugParam is
 * CPA_TRUE.
 */
#define PRINT_DBG(args...)                                                     \
    do                                                                         \
    {                                                                          \
        if (true == gDebugParam)                                           \
        {                                                                      \
            printf("%s(): ", __func__);                                        \
            printf(args);                                                      \
            fflush(stdout);                                                    \
        }                                                                      \
    } while (0)

/**< Prints the arguments */
#ifndef PRINT
#define PRINT(args...)                                                         \
    do                                                                         \
    {                                                                          \
        printf(args);                                                          \
    } while (0)
#endif

/**< Prints the name of the function and the arguments */
#ifndef PRINT_ERR
#define PRINT_ERR(args...)                                                     \
    do                                                                         \
    {                                                                          \
        printf("%s(): ", __func__);                                            \
        printf(args);                                                          \
    } while (0)
#endif


#endif