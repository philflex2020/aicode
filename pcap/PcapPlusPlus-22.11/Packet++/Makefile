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

HASH_LIB_HOME := ../3rdParty/hash-library
HASH_LIB_SOURCES := $(wildcard $(HASH_LIB_HOME)/*.cpp)
HASH_LIB_OBJS_FILENAMES := $(patsubst $(HASH_LIB_HOME)/%.cpp,$(HASH_LIB_HOME)/Obj/%.o,$(HASH_LIB_SOURCES))

COMMONPP_HOME := ../Common++

ifdef WIN32
DEPS := -DWPCAP -DHAVE_REMOTE
endif

INCLUDES := -I"./src" \
			-I"./header" \
			-I"$(COMMONPP_HOME)/header" \
			-I"$(PCAPPLUSPLUS_HOME)/3rdParty/EndianPortable/include" \
			-I"$(HASH_LIB_HOME)"

ifdef BUILD_WITH_STATIC_LIBPCAP
INCLUDES += -I"$(STATIC_LIBPCAP_INCLUDE)"
endif

ifdef WIN32
INCLUDES += -I$(MINGW_HOME)/include/ddk \
			-I$(PCAP_SDK_HOME)/Include
endif

Obj/%.o: src/%.cpp Obj/%.d
	@echo Building file: $<
	@$(CXX) $(DEPS) $(INCLUDES) $(PCAPPP_BUILD_FLAGS) -c -fmessage-length=0 -MMD -MP -MF "$(@:Obj/%.o=Obj/%.d)" -MT "$@" -o "$@" "$<"

DEPFILES := $(OBJS_FILENAMES:Obj/%.o=Obj/%.d)
$(DEPFILES):
include $(wildcard $(DEPFILES))


CUR_TARGET := $(notdir $(shell pwd))

.SILENT:

all: Packet++

start:
	@echo ==== Building target: $(CUR_TARGET) ====

create-directories:
	@$(MKDIR) -p Obj
	@$(MKDIR) -p Lib

Packet++: start create-directories $(OBJS_FILENAMES)
	@cd $(HASH_LIB_HOME) && $(MAKE) hash_library_sources
	@$(AR) -r  "Lib/$(LIB_PREFIX)Packet++$(LIB_EXT)" $(OBJS_FILENAMES) $(HASH_LIB_OBJS_FILENAMES)
	@echo Finished successfully building: $(CUR_TARGET)
	@echo ' '

clean:
	@cd $(HASH_LIB_HOME) && $(MAKE) clean
	@$(RM) -rf ./Obj/*
	@$(RM) -rf ./Lib/*
	@echo Clean finished: $(CUR_TARGET)
