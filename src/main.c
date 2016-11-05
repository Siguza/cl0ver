#include <unistd.h>             // sync

#include "common.h"             // ERROR, log_init, log_release
#include "exploit.h"            // exploit
#include "io.h"                 // OSString

int main(int argc, const char **argv)
{
    // ffffff800445a1dc T _is_io_registry_entry_get_property_bytes
    // ffffff8004536000 S _ipc_kernel_map
    // ffffff8004536370 S __ZN8OSSymbol10gMetaClassE
    // ffffff8004536348 S __ZN8OSString10gMetaClassE

    // 6,1 ffffff80044eda08 S __ZTV11OSMetaClass
    // 8,4 ffffff80044ef460 S __ZTV11OSMetaClass

    log_init(argc > 1 ? argv[1] : NULL);
    if(argc > 2)
    {
        ERROR("Too many arguments");
    }
    sync();

    exploit();

    log_release();
    return 0;
}
