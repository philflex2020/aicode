ifeq ($(wildcard ../mk/platform.mk),)
  $(error platform.mk not found! Please run configure script first)
endif

include ../mk/platform.mk

ifeq ($(wildcard ../mk/PcapPlusPlus.mk),)
  $(error PcapPlusPlus.mk not found! Please run configure script first)
endif

include ../mk/PcapPlusPlus.mk

SOURCES := $(wildcard src/*.cpp)
OBJS_FILENAMES := $(patsubst src/%.cpp,Obj/%.o,$(SOURCES))

COMMONPP_HOME := ../Common++
PACKETPP_HOME := ../Packet++
LIGHT_PCAPNG_PCPP_HOME := ../3rdParty/LightPcapNg
LIGHT_PCAPNG_HOME := $(LIGHT_PCAPNG_PCPP_HOME)/LightPcapNg

LIGHT_PCAPNG_SOURCES := $(wildcard $(LIGHT_PCAPNG_HOME)/src/*.c)
LIGHT_PCAPNG_OBJS_FILENAMES := $(patsubst $(LIGHT_PCAPNG_HOME)/src/%.c,$(LIGHT_PCAPNG_PCPP_HOME)/Obj/%.o,$(LIGHT_PCAPNG_SOURCES))

ifdef WIN32
DEPS := -DWPCAP -DHAVE_REMOTE -DHAVE_STRUCT_TIMESPEC
endif
ifdef PF_RING_HOME
DEPS += -DUSE_PF_RING
endif
ifdef USE_DPDK
DEPS += -DUSE_DPDK
endif

INCLUDES := -I"./src" \
			-I"./header" \
			-I"$(COMMONPP_HOME)/header" \
			-I"$(PACKETPP_HOME)/header" \
			-I"$(LIGHT_PCAPNG_HOME)/include" \
			-I"$(PCAPPLUSPLUS_HOME)/3rdParty/EndianPortable/include"

ifdef WIN32
INCLUDES += -I$(MINGW_HOME)/include/ddk \
			-I$(PCAP_SDK_HOME)/Include
endif
ifdef PF_RING_HOME
INCLUDES += -I$(PF_RING_HOME)/userland/lib -I$(PF_RING_HOME)/kernel
endif
ifdef USE_DPDK
PKGCONF = pkg-config
TMPI = $(shell $(PKGCONF) --cflags-only-I libdpdk 2>/dev/null)
ifneq ($(TMPI),)
INCLUDES += $(TMPI)
else
INCLUDES += -I"$(RTE_SDK)/build/include"
endif
endif
ifdef LIBPCAP_INCLUDE_DIR
INCLUDES += -I"$(LIBPCAP_INCLUDE_DIR)"
endif
ifdef BUILD_WITH_STATIC_LIBPCAP
INCLUDES += -I"$(STATIC_LIBPCAP_INCLUDE)"
endif

ifdef HAS_PCAP_IMMEDIATE_MODE
DEPS += -DHAS_PCAP_IMMEDIATE_MODE
endif

ifdef HAS_SET_DIRECTION_ENABLED
PCAPPP_BUILD_FLAGS += -DHAS_SET_DIRECTION_ENABLED
endif

ifdef USE_DPDK
PCAPPP_BUILD_FLAGS += -msse -msse2 -msse3 -mssse3
endif

Obj/%.o: src/%.cpp Obj/%.d
	@echo Building file: $<
	@$(CXX) $(DEPS) $(INCLUDES) $(PCAPPP_BUILD_FLAGS) -c -fmessage-length=0 -MMD -MP -MF "$(@:Obj/%.o=Obj/%.d)" -MT "$@" -o "$@" "$<"

DEPFILES := $(OBJS_FILENAMES:Obj/%.o=Obj/%.d)
$(DEPFILES):
include $(wildcard $(DEPFILES))

CUR_TARGET := $(notdir $(shell pwd))

.SILENT:

all: Pcap++.lib

start:
	@echo ==== Building target: $(CUR_TARGET) ====

create-directories:
	@$(MKDIR) -p Obj
	@$(MKDIR) -p Lib

Pcap++.lib: start create-directories $(OBJS_FILENAMES)
	@cd $(LIGHT_PCAPNG_PCPP_HOME) && $(MAKE) light_pcapng_sources
	@$(AR) -r  "Lib/$(LIB_PREFIX)Pcap++$(LIB_EXT)" $(OBJS_FILENAMES) $(LIGHT_PCAPNG_OBJS_FILENAMES)
	@echo Finished successfully building: $(CUR_TARGET)
	@echo ' '

clean:
	@cd $(LIGHT_PCAPNG_PCPP_HOME) && $(MAKE) clean
	@$(RM) -rf ./Obj/*
	@$(RM) -rf ./Lib/*
	@echo Clean finished: $(CUR_TARGET)
