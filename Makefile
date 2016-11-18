TARGET = cl0ver
INCDIR = include
MIGDIR = mig
SRCDIR = src
OBJDIR = obj
LIBKERN ?= /usr/include
OSFMK ?= /usr/include
FRAMEWORKS ?= /System/Library/Frameworks/
IGCC ?= xcrun -sdk iphoneos gcc
IGCC_FLAGS = -arch armv7 -arch arm64 -Wall -O3 -std=c11 -fmodules -I./$(SRCDIR)/lib -I./$(INCDIR) -I./$(MIGDIR) $(CFLAGS)
LD_FLAGS = -Wl,-dead_strip -L. $(LDFLAGS)
LD_LIBS = -framework IOKit -l$(TARGET) $(LIBS)
SIGN ?= xcrun -sdk iphoneos codesign
SIGN_FLAGS ?= -s -
MIG ?= xcrun -sdk iphoneos mig
MIG_FLAGS ?= -arch arm64 -DIOKIT -I../$(INCDIR)
LIBTOOL ?= xcrun -sdk iphoneos libtool
LIBTOOL_FLAGS ?= -static

.PHONY: all lib clean fullclean

all: $(TARGET)

lib: lib$(TARGET).a

$(TARGET): lib$(TARGET).a $(INCDIR) $(MIGDIR)
	$(IGCC) -o $@ $(IGCC_FLAGS) $(LD_FLAGS) $(LD_LIBS) $(SRCDIR)/cli/*.c
	$(SIGN) $(SIGN_FLAGS) $(TARGET)

lib$(TARGET).a: $(patsubst $(SRCDIR)/lib/%.c,$(OBJDIR)/%.o,$(wildcard $(SRCDIR)/lib/*.c))
	$(LIBTOOL) $(LIBTOOL_FLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/lib/%.c $(INCDIR) $(MIGDIR) | $(OBJDIR)
	$(IGCC) -c -o $@ $(IGCC_FLAGS) $<

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

$(OBJDIR):
	mkdir $(OBJDIR)

clean:
	rm -rf $(TARGET) lib$(TARGET).a $(INCDIR) $(OBJDIR)

fullclean: clean
	rm -rf $(MIGDIR)
