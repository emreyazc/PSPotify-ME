TARGET = bg_user_canary
OBJS = canary.o
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
PSP_FW_VERSION = 500
CFLAGS = -Os -G0 -Wall -Wextra -std=c99
LDFLAGS = -nostartfiles
LIBS = -lpspuser
PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
