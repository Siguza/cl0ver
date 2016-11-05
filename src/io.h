#ifndef IO_H
#define IO_H

#include <stddef.h>             // size_t
#include <stdint.h>             // uint32_t

enum { kOSStringNoCopy = 0x00000001 };

typedef void** vtab_t;

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
    char       * string;
} OSString;

void dict_get_property_bytes(void *dict, size_t dictlen, const char *key, char *buf, uint32_t *buflen);

void dict_parse(void *dict, size_t dictlen);

#endif
