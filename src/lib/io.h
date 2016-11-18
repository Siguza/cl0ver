#ifndef IO_H
#define IO_H

#include <stddef.h>             // size_t
#include <stdint.h>             // uint32_t

#include <IOKit/IOKitLib.h>     // io_service_t

#define MIG_MSG_SIZE 0x1000

enum
{
    kOSSerializeDictionary      = 0x01000000U,
    kOSSerializeArray           = 0x02000000U,
    kOSSerializeSet             = 0x03000000U,
    kOSSerializeNumber          = 0x04000000U,
    kOSSerializeSymbol          = 0x08000000U,
    kOSSerializeString          = 0x09000000U,
    kOSSerializeData            = 0x0a000000U,
    kOSSerializeBoolean         = 0x0b000000U,
    kOSSerializeObject          = 0x0c000000U,

    kOSSerializeTypeMask        = 0x7F000000U,
    kOSSerializeDataMask        = 0x00FFFFFFU,

    kOSSerializeEndCollection   = 0x80000000U,

    kOSSerializeMagic           = 0x000000d3U,
};

enum
{
    kOSStringNoCopy = 0x00000001,
};

typedef const void ** vtab_t;

typedef struct
{
    vtab_t       vtab;
    int          retainCount;
    void       * data;
    unsigned int length;
    unsigned int capacity;
    unsigned int capacityIncrement;
    void       * reserved;
} OSData;

typedef struct
{
    vtab_t       vtab;
    int          retainCount;
    unsigned int flags;
    unsigned int length;
    const char * string;
} OSString;

/* Building blocks */

io_service_t _io_get_service(const char *name);

io_connect_t _io_spawn_client(io_service_t service, void *dict, size_t dictlen);

void _io_get_bytes(io_service_t service, const char *key, void *buf, uint32_t *buflen);

void _io_release_client(io_connect_t client);

/* All-in-one routines */

void dict_get_bytes(void *dict, size_t dictlen, const char *key, void *buf, uint32_t *buflen);

void dict_parse(void *dict, size_t dictlen);

#endif
