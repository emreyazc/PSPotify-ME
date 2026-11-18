# BG-14 user-mode prototype, not included in GAME plugin build.
TARGET = pspotify_bg_net
OBJS = net_worker.o
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
PSP_FW_VERSION = 500

CFLAGS = -Os -G0 -Wall -Wextra -std=c99
LIBS = -lpspnet_apctl -lpspnet_inet -lpspnet_resolver -lpspnet -lpspwlan -lpsputility -lpspuser

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
