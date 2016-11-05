#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>              // FILE, fprintf
#include <stdlib.h>             // exit

#ifdef __OBJC__

#   import <Foundation/Foundation.h>

#   define LOG(str, args...) \
    do \
    { \
        if(logfile == NULL) \
            NSLog(@str, args); \
        else \
            fprintf(logfile, str, ##args); \
    } while(0)

#else

#   include <syslog.h>

#   define LOG(str, args...) \
    do \
    { \
        if(logfile == NULL) \
            syslog(LOG_WARNING, str, args); \
        else \
            fprintf(logfile, str, ##args); \
    } while(0)

#endif // __OBJC__

extern FILE *logfile;

void log_init(const char *file);

void log_release();

#define ASSERT(expr) \
do \
{ \
    if(!(expr)) \
    { \
        ERROR("Assertion failed: " #expr ); \
    } \
} while(0)

#define DEBUG(str, args...) \
do \
{ \
    LOG("[DEBUG] " str " [" __FILE__ ":%u]\n", ##args, __LINE__); \
} while(0)

#define ERROR(str, args...) \
do \
{ \
    LOG("[ERROR] " str " [" __FILE__ ":%u]\n", ##args, __LINE__); \
    exit(1); \
} while(0)

#ifdef __LP64__
#   define ADDR "0x%016llx"
#else
#   define ADDR "0x%08x"
#endif

#define SIZE "0x%016lx"

#endif
