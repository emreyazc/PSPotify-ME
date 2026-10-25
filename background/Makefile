# Only builds the standalone GAME plugin. No EBOOT changes.
TARGET = pspotify_bg
OBJS = pspotify_bg.o
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
PSP_FW_VERSION = 500

USE_KERNEL_LIBS = 1
USE_KERNEL_LIBC = 1
CFLAGS = -Os -G0 -Wall -Wextra -std=c99 -fno-builtin-printf
LDFLAGS = -nostartfiles
LIBS = -lpspaudio -lpspctrl_driver -lpspkernel -lc

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
