#include <stdint.h>             // uint32_t, uint64_t

#include <mach/kern_return.h>   // kern_return_t, KERN_SUCCESS
#include <mach/mach_error.h>    // mach_error_string
#include <mach/mach_host.h>     // host_get_io_master, mach_port_t
#include <mach/mach_traps.h>    // mach_host_self
#include <mach/message.h>       // mach_msg_type_number_t
#include <mach/port.h>          // MACH_PORT_NULL, MACH_PORT_VALID

#include <IOKit/IOKitLib.h>     // IO*, io_*
#ifndef __LP64__
#   include <IOKit/iokitmig.h>  // io_service_open_extended
#else
// Yes, I'm including a .c file. Sue me.
#   include "iokitUser.c"       // io_service_open_extended
#endif

#include "common.h"             // DEBUG, ERROR

#include "io.h"

/* Helper functions */

static mach_port_t get_io_master_port()
{
    static mach_port_t master = MACH_PORT_NULL;
    if(master == MACH_PORT_NULL)
    {
        DEBUG("Getting IO master port...");
        kern_return_t ret = host_get_io_master(mach_host_self(), &master);
        if(ret != KERN_SUCCESS || !MACH_PORT_VALID(master))
        {
            ERROR("Failed to get IO master port (port = 0x%08x, ret = %u: %s)", master, ret, mach_error_string(ret));
        }
    }
    return master;
}

/* Building blocks */

io_service_t _io_get_service(const char *name)
{
    // TODO: static?
    DEBUG("Getting IO service handle...");
    io_service_t service = IOServiceGetMatchingService(get_io_master_port(), IOServiceMatching(name == NULL ? "AppleKeyStore" : name));
    if(!MACH_PORT_VALID(service))
    {
        ERROR("Failed to get IO service handle (port = 0x%08x, name= %s)", service, name);
    }

    return service;
}

io_connect_t _io_spawn_client(io_service_t service, void *dict, size_t dictlen)
{
    DEBUG("Spawning user client / Parsing dictionary...");
    io_connect_t client = MACH_PORT_NULL;
    kern_return_t err;
    kern_return_t ret = io_service_open_extended(service, mach_task_self(), 0, NDR_record, dict, dictlen, &err, &client);
    if(ret != KERN_SUCCESS || err != KERN_SUCCESS || !MACH_PORT_VALID(client))
    {
        ERROR("Failed to parse dictionary (client = 0x%08x, ret = %u: %s, err = %u: %s)", client, ret, mach_error_string(ret), err, mach_error_string(err));
    }
    return client;
}

void _io_get_bytes(io_service_t service, const char *key, void *buf, uint32_t *buflen)
{
    kern_return_t ret;

    DEBUG("Creating dict iterator...");
    io_iterator_t it = 0;
    ret = IORegistryEntryCreateIterator(service, "IOService", kIORegistryIterateRecursively, &it);
    if(ret != KERN_SUCCESS)
    {
        ERROR("Failed to create iterator (ret = %u: %s)", ret, mach_error_string(ret));
    }

    DEBUG("Getting next element from iterator...");
    io_object_t o = IOIteratorNext(it);
    if(o == 0)
    {
        ERROR("Failed to get next element (o = %u)", o);
    }

    DEBUG("Retrieving bytes...");
    ret = IORegistryEntryGetProperty(o, key, buf, buflen);
    if(ret != KERN_SUCCESS)
    {
        ERROR("Failed to get bytes (ret = %u: %s)", ret, mach_error_string(ret));
    }

    IOObjectRelease(o);
    IOObjectRelease(it);
}

void _io_release_client(io_connect_t client)
{
    DEBUG("Releasing user client...");
    kern_return_t ret = IOServiceClose(client);
    if(ret != KERN_SUCCESS)
    {
        ERROR("Failed to release user client (ret = %u: %s)", ret, mach_error_string(ret));
    }
}

/* All-in-one routines */

void dict_get_bytes(void *dict, size_t dictlen, const char *key, void *buf, uint32_t *buflen)
{
    io_service_t service = _io_get_service("IOHDIXController"); // TODO
    //io_service_t service = _io_get_service(NULL);
    io_connect_t client = _io_spawn_client(service, dict, dictlen);
    _io_get_bytes(service, key, buf, buflen);
    _io_release_client(client);
}

void dict_parse(void *dict, size_t dictlen)
{
    io_service_t service = _io_get_service(NULL);
    io_connect_t client = _io_spawn_client(service, dict, dictlen);
    _io_release_client(client);
}
