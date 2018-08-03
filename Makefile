# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# Include the VCV Rack Architecture
include $(RACK_DIR)/arch.mk

# Must follow the format in the Naming section of
# https://vcvrack.com/manual/PluginDevelopmentTutorial.html
SLUG = Modiy

# Must follow the format in the Versioning section of
# https://vcvrack.com/manual/PluginDevelopmentTutorial.html
VERSION = 0.1.0

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -Isrc/
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine.
LDFLAGS += 

# Add .cpp and .c files to the build
SOURCES += $(wildcard src/*.cpp)
# OSC
SOURCES += $(wildcard src/ip/*.cpp) $(wildcard src/osc/*.cpp)

# Arch specitic files
ifdef ARCH_LIN
SOURCES += $(wildcard src/ip/posix/*.cpp)
endif
ifdef ARCH_MAC
SOURCES += $(wildcard src/ip/posix/*.cpp)
endif
ifdef ARCH_WIN
SOURCES += $(wildcard src/ip/win32/*.cpp)
endif


# Add files to the ZIP package when running `make dist`
# The compiled plugin is automatically added.
DISTRIBUTABLES += $(wildcard LICENSE*) res

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
