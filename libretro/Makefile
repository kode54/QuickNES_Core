DEBUG = 0

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
EXE_EXT = .exe
   system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   system_platform = osx
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   system_platform = win
endif

LIBRETRO_DIR := libretro

ifeq ($(platform), unix)
   TARGET := libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,-version-script=libretro/link.T
else ifeq ($(platform), osx)
   TARGET := libretro.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
else ifeq ($(platform), ps3)
   TARGET := libretro.a
   CXX = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-g++.exe
   AR = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-ar.exe
   PLATFORM_DEFINES := -D__CELLOS_LV2__
else ifeq ($(platform), sncps3)
   TARGET := libretro_ps3.a
   CC = $(CELL_SDK)/host-win32/sn/bin/ps3ppusnc.exe
   CXX = $(CELL_SDK)/host-win32/sn/bin/ps3ppusnc.exe
   AR = $(CELL_SDK)/host-win32/sn/bin/ps3snarl.exe
   PLATFORM_DEFINES := -D__CELLOS_LV2__
else ifeq ($(platform), psl1ght)
   TARGET := libretro_psl1ght.a
   CC = $(PS3DEV)/ppu/bin/ppu-gcc$(EXE_EXT)
   CXX = $(PS3DEV)/ppu/bin/ppu-g++$(EXE_EXT)
   AR = $(PS3DEV)/ppu/bin/ppu-ar$(EXE_EXT)
   PLATFORM_DEFINES := -D__CELLOS_LV2__
else ifeq ($(platform), xenon)
   TARGET := libretro_xenon360.a
   CC = xenon-gcc$(EXE_EXT)
   CXX = xenon-g++$(EXE_EXT)
   AR = xenon-ar$(EXE_EXT)
   PLATFORM_DEFINES := -D__LIBXENON__
else ifeq ($(platform), ngc)
   TARGET := libretro_ngc.a
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   PLATFORM_DEFINES += -DGEKKO -DHW_DOL -mrvl -mcpu=750 -meabi -mhard-float
else ifeq ($(platform), wii)
   TARGET := libretro_wii.a
   CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
   CXX = $(DEVKITPPC)/bin/powerpc-eabi-g++$(EXE_EXT)
   AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)
   PLATFORM_DEFINES += -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float
else
   TARGET := retro.dll
   CC = gcc
   CXX = g++
   SHARED := -shared -static-libgcc -static-libstdc++ -Wl,-no-undefined -Wl,-version-script=libretro/link.T
endif

all: $(TARGET)

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g
   CXXFLAGS += -O0 -g
else
   CFLAGS += -O3
   CXXFLAGS += -O3
endif

EMU_DIR := ../nes_emu

CXXSRCS := \
	$(EMU_DIR)/abstract_file.cpp \
	$(EMU_DIR)/apu_state.cpp \
	$(EMU_DIR)/Blip_Buffer.cpp \
	$(EMU_DIR)/Effects_Buffer.cpp \
	$(EMU_DIR)/Mapper_Fme7.cpp \
	$(EMU_DIR)/Mapper_Mmc5.cpp \
	$(EMU_DIR)/Mapper_Namco106.cpp \
	$(EMU_DIR)/Mapper_Vrc6.cpp \
	$(EMU_DIR)/misc_mappers.cpp \
	$(EMU_DIR)/Multi_Buffer.cpp \
	$(EMU_DIR)/Nes_Apu.cpp \
	$(EMU_DIR)/Nes_Buffer.cpp \
	$(EMU_DIR)/Nes_Cart.cpp \
	$(EMU_DIR)/Nes_Core.cpp \
	$(EMU_DIR)/Nes_Cpu.cpp \
	$(EMU_DIR)/nes_data.cpp \
	$(EMU_DIR)/Nes_Effects_Buffer.cpp \
	$(EMU_DIR)/Nes_Emu.cpp \
	$(EMU_DIR)/Nes_File.cpp \
	$(EMU_DIR)/Nes_Film.cpp \
	$(EMU_DIR)/Nes_Film_Data.cpp \
	$(EMU_DIR)/Nes_Film_Packer.cpp \
	$(EMU_DIR)/Nes_Fme7_Apu.cpp \
	$(EMU_DIR)/Nes_Mapper.cpp \
	$(EMU_DIR)/nes_mappers.cpp \
	$(EMU_DIR)/Nes_Mmc1.cpp \
	$(EMU_DIR)/Nes_Mmc3.cpp \
	$(EMU_DIR)/Nes_Namco_Apu.cpp \
	$(EMU_DIR)/nes_ntsc.c \
	$(EMU_DIR)/Nes_Oscs.cpp \
	$(EMU_DIR)/Nes_Ppu.cpp \
	$(EMU_DIR)/Nes_Ppu_Impl.cpp \
	$(EMU_DIR)/Nes_Ppu_Rendering.cpp \
	$(EMU_DIR)/Nes_Recorder.cpp \
	$(EMU_DIR)/Nes_State.cpp \
	$(EMU_DIR)/nes_util.cpp \
	$(EMU_DIR)/Nes_Vrc6_Apu.cpp \
	libretro.cpp

CXXOBJ := $(CXXSRCS:.cpp=.o)

OBJS := $(CXXOBJ)

DEFINES := -D__LIBRETRO__ $(PLATFORM_DEFINES) -Wall -Wno-multichar

CFLAGS += $(fpic) $(DEFINES) -std=gnu99
CXXFLAGS += $(fpic) $(DEFINES)

INCDIRS := -I$(EMU_DIR) -I. -I..

$(TARGET): $(OBJS)
ifeq ($(platform), ps3)
	$(AR) rcs $@ $(OBJS)
else ifeq ($(platform), sncps3)
	$(AR) rcs $@ $(OBJS)
else ifeq ($(platform), psl1ght)
	$(AR) rcs $@ $(OBJS)
else ifeq ($(platform), xenon)
	$(AR) rcs $@ $(OBJS)
else ifeq ($(platform), ngc)
	$(AR) rcs $@ $(OBJS)
else ifeq ($(platform), wii)
	$(AR) rcs $@ $(OBJS)
else
	$(CXX) -o $@ $(SHARED) $(OBJS) $(LDFLAGS) $(LIBS)
endif

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(INCDIRS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(INCDIRS)

clean-objs:
	rm -f $(OBJS)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean clean-objs
