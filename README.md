# cl0ver

A tfp0 patch for iOS 9, based on the Pegasus/Trident vulnerabilities.

### Building

On macOS with XCode:

    make
    
On a different OS with an iOS SDK and `ldid` installed:

*   Download a [XNU source tarball](https://opensource.apple.com/tarballs/xnu/) and unzip it.
*   Download an [IOKitUser source tarball](https://opensource.apple.com/tarballs/IOKitUser/) and unzip it.
*   Export the following environment variables:

        LIBKERN=path/to/xnu/libkern
        OSFMK=path/to/xnu/osfmk
        IOKIT=path/to/IOKitUser
        IGCC=ios-compiler-command
        LIBTOOL=ios-libtool-command
        SIGN=ldid
        SIGN_FLAGS=-S
    
### Usage

    ./cl0ver panic [log=file]
        Panic the device, loading to PC:
        on 32-bit: the base address of __DATA.__const
        on 64-bit: the OSString vtable
    
    ./cl0ver slide [log=file]
        Print kernel slide
        
    ./cl0ver dump [log=file]
        Dump kernel to kernel.bin
    
    ./cl0ver [log=file]
        Apply tfp0 kernel patch
        
    If log=file is give, output is written to "file" instead of stderr/syslog.
    
### GUI/Sandbox

This repo doesn't contain any code for a GUI/Sandbox app, but a `libcl0ver.a` is built, which can be linked against. You'll most likely wanna call functions from `exploit.h`.  
And you'll want to call them like:

    dump_kernel([[NSHomeDirectory() stringByAppendingPathComponent:@"Documents"] stringByAppendingPathComponent:@"kernel.bin"].UTF8String);
    // or
    get_kernel_task([NSHomeDirectory() stringByAppendingPathComponent:@"Documents"].UTF8String);

### Config/Cache

If you want to run this on a device whose OSString vtable and stack anchor are not in the registry, create a file at `/etc/cl0ver/config.txt`, containing in hexadecimal the stack anchor on line 1 and the unslid OSString vtable address on line 2.

Also, make sure `/etc/cl0ver/` exists and is writable by the current user, if you want offsets to get cached.

# Writeup

**[ [Here](https://siguza.github.io/cl0ver/) ]**

# License

Unless otherwise noted at the top of the file, all files in this repository are released under the [MIT License](LICENSE).
