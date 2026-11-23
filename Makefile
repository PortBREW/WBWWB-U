TARGET      := wbwwb
BUILD       := build
SOURCES     := src
OFILES      := main.o
DEPSDIR     := $(BUILD)
VPATH       := $(SOURCES)

APP_NAME      := We Become What We Behold
APP_SHORTNAME := WBWWB
APP_AUTHOR    := PortBrew
APP_CONTENT   := assets
APP_ICON      := icon.png
APP_TV_SPLASH := splash.png
APP_DRC_SPLASH := drc_splash.png

CFLAGS   = $(MACHDEP) -O2 -Wall -Wextra -ffunction-sections -fdata-sections
CXXFLAGS = $(CFLAGS) -std=gnu++17 -fno-exceptions -fno-rtti
CPPFLAGS = $(MACHDEP) $(RPXSPECS) -I$(DEVKITPRO)/portlibs/wiiu/include/SDL2 -I$(WUT_ROOT)/include
LDFLAGS  = $(MACHDEP) $(RPXSPECS) -Wl,--gc-sections -L$(DEVKITPRO)/portlibs/wiiu/lib -L$(WUT_ROOT)/lib
LIBS     = -lSDL2 -lwut

.PHONY: all clean
all: $(TARGET).wuhb

$(BUILD):
	mkdir -p $@

main.o: main.cpp | $(BUILD)
$(TARGET).elf: $(OFILES)

clean:
	rm -rf $(BUILD) main.o $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb $(TARGET).lst

include $(DEVKITPRO)/wut/share/wut_rules
LD := $(CXX)
