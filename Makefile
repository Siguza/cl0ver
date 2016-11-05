TARGET = tfp0
INCDIR = include
MIGDIR = mig
LIBKERN ?= /usr/include
OSFMK ?= /usr/include
FRAMEWORKS ?= /System/Library/Frameworks/
IGCC ?= xcrun -sdk iphoneos gcc
IGCC_FLAGS = -arch armv7 -arch arm64 -Wall -O3 -std=c99 -I./$(INCDIR) -I./$(MIGDIR) -fmodules -framework IOKit $(CFLAGS)
SIGN ?= xcrun -sdk iphoneos codesign
SIGN_FLAGS ?= -s -
MIG ?= xcrun -sdk iphoneos mig
MIG_FLAGS ?= -arch arm64 -DIOKIT -I../$(INCDIR)

.PHONY: all clean fullclean

all: $(INCDIR) $(MIGDIR)
	$(IGCC) -o $(TARGET) $(IGCC_FLAGS) src/*.c
	$(SIGN) $(SIGN_FLAGS) $(TARGET)

$(INCDIR):
	mkdir $(INCDIR)
	ln -s $(FRAMEWORKS)/IOKit.framework/Headers $(INCDIR)/IOKit
	mkdir $(INCDIR)/libkern
	ln -s $(LIBKERN)/libkern/OSTypes.h $(INCDIR)/libkern/OSTypes.h
	mkdir $(INCDIR)/mach
	ln -s $(OSFMK)/mach/clock_types.defs $(INCDIR)/mach/clock_types.defs
	ln -s $(OSFMK)/mach/mach_types.defs $(INCDIR)/mach/mach_types.defs
	ln -s $(OSFMK)/mach/std_types.defs $(INCDIR)/mach/std_types.defs
	mkdir $(INCDIR)/mach/machine
	ln -s $(OSFMK)/mach/machine/machine_types.defs $(INCDIR)/mach/machine/machine_types.defs

$(MIGDIR): | $(INCDIR)
	mkdir $(MIGDIR)
	cd $(MIGDIR) && $(MIG) $(MIG_FLAGS) $(OSFMK)/device/device.defs

clean:
	rm -rf $(TARGET) $(INCDIR)

fullclean: clean
	rm -rf $(MIGDIR)
