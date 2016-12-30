# cl0ver

A tfp0 patch for iOS 9, based on the Pegasus/Trident vulnerabilities.

### Building

On macOS with XCode and XCode command line tools installed:

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

### Device/OS Support

Only a small set of device/OS combinations is currently supported.  
If you would like me to add support for yours, please run `./cl0ver panic` and [open a ticket](https://github.com/Siguza/cl0ver/issues/new) containing cl0ver's output as well as the panic log.

### Config/Cache

If you know stack anchor and OSString vtable for an unsupported device/OS and don't want to wait for me to add support for it, you can do the following:  
Create a file at `/etc/cl0ver/config.txt` containing in hexadecimal: on line 1 the stack anchor, on line 2 the OSString vtable address.

If you want a dumped kernel to be saved, and calculated offsets to be cached, make sure the directory `/etc/cl0ver` exists and is writeable by the current user.

If you have a dumped/decrypted kernel and want to skip kernel dumping, place it at `/etc/kernel.bin`.

### GUI/Sandbox

This repo doesn't contain any code for a GUI/Sandbox app, but a `libcl0ver.a` is built, which can be linked against. You'll most likely want to call functions from `exploit.h`.  
And you'll want to call them like:

    dump_kernel([[NSHomeDirectory() stringByAppendingPathComponent:@"Documents"] stringByAppendingPathComponent:@"kernel.bin"].UTF8String);
    // or
    get_kernel_task([NSHomeDirectory() stringByAppendingPathComponent:@"Documents"].UTF8String);

# Writeup

**[ [Here](https://siguza.github.io/cl0ver/) ]**

# License

Unless otherwise noted at the top of the file, all files in this repository are released under the [MIT License](LICENSE).
