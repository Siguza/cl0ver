#include <errno.h>              // errno
#include <stdbool.h>            // bool, true
#include <stdio.h>              // FILE, stderr, fopen, fclose
#include <string.h>             // strerror
#include <unistd.h>             // getppid

#include "io.h"                 // OSString
#include "try.h"                // THROW

#include "common.h"

bool verbose = true;

FILE *logfile = NULL;

void log_init(const char *file)
{
    logfile = stderr;
    if(getppid() == 1) // GUI mode
    {
        logfile = NULL;
    }
    else if(file != NULL)
    {
        FILE *f = fopen(file, "wb");
        if(f == NULL)
        {
            THROW("Failed to open logfile (%s)", strerror(errno));
        }
        logfile = f;
    }
}

void log_release(void)
{
    if(logfile != NULL && logfile != stderr)
    {
        fclose(logfile);
        log_init(NULL);
    }
}

void sanity(void)
{
    // In case we panic...
    sync();

#ifdef __LP64__
    ASSERT(sizeof(OSString) == 8 * sizeof(uint32_t));
#else
    ASSERT(sizeof(OSString) == 5 * sizeof(uint32_t));
#endif

    // TODO: sysctl("hw.cputype")
}
