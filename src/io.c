#include <stdint.h>             // uint32_t, uint64_t

#include <mach/kern_return.h>   // kern_return_t, KERN_SUCCESS
#include <mach/mach_error.h>    // mach_error_string
#include <mach/mach_host.h>     // host_get_io_master, mach_port_t
#include <mach/mach_traps.h>    // mach_host_self
#include <mach/message.h>       // mach_msg_type_number_t
#include <mach/port.h>          // MACH_PORT_NULL

#include <IOKit/IOKitLib.h>     // IO*, io_*
#ifndef __LP64__
#   include <IOKit/iokitmig.h>  // io_service_open_extended
#else
// Yes, I'm including a .c file. Sue me.
#   include "iokitUser.c"       // io_service_open_extended
#endif

#include "common.h"             // DEBUG, ERROR

static mach_port_t get_io_master_port()
{
    DEBUG("Getting IO master port...");
    mach_port_t master = MACH_PORT_NULL;
    kern_return_t ret = host_get_io_master(mach_host_self(), &master);
    if(ret != KERN_SUCCESS || !MACH_PORT_VALID(master))
    {
        ERROR("Failed to get IO master port (port = 0x%08x, ret = %u: %s)", master, ret, mach_error_string(ret));
    }
    return master;
}

void dict_get_property_bytes(void *dict, size_t dictlen, const char *key, char *buf, uint32_t *buflen)
{
    kern_return_t ret;
    mach_port_t master = get_io_master_port();

    DEBUG("Spawning user client...");
    io_service_t client = IOServiceGetMatchingService(master, IOServiceMatching("IOHDIXController"));
    if(!MACH_PORT_VALID(client))
    {
        ERROR("Failed to spawn user client (port = 0x%08x)", client);
    }

    DEBUG("Parsing dictionary...");
    kern_return_t err;
    io_connect_t dummy = MACH_PORT_NULL;
    ret = io_service_open_extended(client, mach_task_self(), 0, NDR_record, dict, dictlen, &err, &dummy);
    if(ret != KERN_SUCCESS || err != KERN_SUCCESS)
    {
        ERROR("Failed to parse dictionary (ret = %u: %s, err = %u: %s)", ret, mach_error_string(ret), err, mach_error_string(err));
    }

    DEBUG("Creating dict iterator...");
    io_iterator_t it = MACH_PORT_NULL;
    ret = IORegistryEntryCreateIterator(client, "IOService", kIORegistryIterateRecursively, &it);
    if(ret != KERN_SUCCESS)
    {
        ERROR("Failed to create iterator (ret = %u: %s)", ret, mach_error_string(ret));
    }

    DEBUG("Retrieving bytes...");
    ret = IORegistryEntryGetProperty(IOIteratorNext(it), key, buf, buflen);
    if(ret != KERN_SUCCESS)
    {
        ERROR("Failed to get bytes (ret = %u: %s)", ret, mach_error_string(ret));
    }
}

void dict_parse(void *dict, size_t dictlen)
{
    mach_port_t master = get_io_master_port();

    DEBUG("Parsing dictionary...");
    io_service_t client = MACH_PORT_NULL;
    kern_return_t ret = io_service_get_matching_service_bin(master, dict, dictlen, &client);
    if(ret != KERN_SUCCESS)
    {
        ERROR("Failed to parse dictionary (ret = %u: %s)", ret, mach_error_string(ret));
    }
}
