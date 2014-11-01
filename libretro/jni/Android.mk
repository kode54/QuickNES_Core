LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS +=  -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS
endif

LOCAL_MODULE    := libretro

CORE_DIR := ../..

include $(CORE_DIR)/libretro/Makefile.common

LOCAL_SRC_FILES    =  $(SOURCES_CXX)
LOCAL_CXXFLAGS = -DANDROID -D__LIBRETRO__ -Wall -Wno-multichar -Wno-unused-variable -Wno-sign-compare -DNDEBUG \
	-DSTD_AUTO_FILE_WRITER=Std_File_Writer \
	-DSTD_AUTO_FILE_READER=Std_File_Reader \
	-DSTD_AUTO_FILE_COMP_READER=Std_File_Reader \
	-DSTD_AUTO_FILE_COMP_WRITER=Std_File_Writer $(INCFLAGS)
LOCAL_C_INCLUDES = $(INCFLAGS)

include $(BUILD_SHARED_LIBRARY)
