#include <errno.h>              // errno
#include <stdio.h>              // FILE, stderr, fopen, fclose
#include <string.h>             // strerror
#include <unistd.h>             // getppid

#include "common.h"

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
            ERROR("Failed to open logfile: %s", strerror(errno));
        }
        logfile = f;
    }
}

void log_release()
{
    if(logfile != NULL && logfile != stderr)
    {
        fclose(logfile);
    }
}
