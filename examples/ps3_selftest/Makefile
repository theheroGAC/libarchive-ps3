TITLE := libarchive PS3 selftest
APPID := LIBARC001
CONTENTID := UP0001-LIBARC001_00-0000000000000000
TARGET := libarchive_ps3_test

LIBARCHIVE ?= ../..
LIBARCHIVE_PREFIX ?= $(LIBARCHIVE)/dist/ps3
PS3DEV ?= /usr/local/ps3dev
PSL1GHT ?= $(PS3DEV)
PORTLIBS ?= $(PS3DEV)/portlibs/ppu

export PS3DEV
export PSL1GHT

CFLAGS := -O2 -Wall -Wextra -Wno-unused-parameter -mcpu=cell -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections -I$(LIBARCHIVE_PREFIX)/include -I$(PORTLIBS)/include -I$(PSL1GHT)/ppu/include
LDFLAGS := -mcpu=cell -mhard-float -Wl,--gc-sections -L$(LIBARCHIVE_PREFIX)/lib -L$(PORTLIBS)/lib -L$(PSL1GHT)/ppu/lib
LD := $(PS3DEV)/ppu/bin/powerpc64-ps3-elf-gcc

# PicoDrive PSL1GHT SDL2 renderer + dependencies
LIBS := -larchive -lz -lbz2 -llzma -lzstd -llz4 SDL_PSL1GHTrender.o -lSDL2 -lgcm_sys -lio -laudio -lsysutil -lrsx -lrt -llv2 -lm -lpng
OFILES := main.o

PKGFILES := pkgfiles

all: $(TARGET).self

pkg: $(TARGET).pkg

include $(PSL1GHT)/ppu_rules

# Override pkg.py with the native pkg binary (matching PicoDrive build)
PKG := pkg

clean:
	rm -f main.o $(TARGET).elf $(TARGET).self $(TARGET).fake.self $(TARGET).pkg $(TARGET).gnpdrm.pkg
	rm -rf build
