#ifndef COMMON_H
#define COMMON_H

#include <math.h>               // floor, log10
#include <stdbool.h>            // bool
#include <stdio.h>              // FILE, fprintf
#include <stdlib.h>             // exit

#include <mach-o/loader.h>      //

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

extern bool verbose;

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
    if(verbose) \
        LOG("[*] " str " [" __FILE__ ":%u %s]\n", ##args, __LINE__, __func__); \
} while(0)

#define ERROR(str, args...) \
do \
{ \
    LOG("[!] " str " [" __FILE__ ":%u %s]\n", ##args, __LINE__, __func__); \
    exit(1); \
} while(0)

#define PRINT_BUF(name, buf, buflen) \
do \
{ \
    DEBUG(name ":"); \
    uint32_t max = (buflen) / sizeof(*(buf)), \
             digits = (uint32_t)floor(log10(max - 1)) + 1; \
    for(uint32_t i = 0; i < max; ++i) \
    { \
        DEBUG( #buf "[%*i]: 0x%0*llx", digits, i, (int)(2 * sizeof(*(buf))), (uint64_t)(buf)[i]); \
    } \
} while(0)

#ifdef __LP64__
#   define ADDR "0x%016llx"
    typedef uint64_t addr_t;
#   define MACH_MAGIC MH_MAGIC_64
    typedef struct mach_header_64 mach_hdr_t;
    typedef struct segment_command_64 mach_seg_t;
#else
#   define ADDR "0x%08x"
    typedef uint32_t addr_t;
#   define MACH_MAGIC MH_MAGIC
    typedef struct mach_header mach_hdr_t;
    typedef struct segment_command mach_seg_t;
#endif

#define SIZE "0x%016lx"
typedef struct load_command mach_cmd_t;
typedef struct
{
    char *buf;
    size_t len;
} file_t;

#endif
