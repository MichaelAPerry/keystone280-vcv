RACK_DIR ?= ../Rack-SDK

# A compiler prefix is optional. The SDK must match the target platform.
ifneq ($(strip $(CROSS)),)
CC := $(CROSS)gcc
CXX := $(CROSS)g++
STRIP := $(CROSS)strip
endif

FLAGS += -Iengine
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res LICENSE

include $(RACK_DIR)/plugin.mk

CXXFLAGS += -std=c++17
