target := bsnes
binary := application
build := performance_debug
openmp := true
local := false
script_profiler := false
discord := true
flags += -I. -I..

# in order for this to work, obj/lzma.o must be omitted or bsnes will hang on startup.
# further, only the X-Video driver works reliably. OpenGL 3.2, OpenGL 2.0, and XShm crash bsnes.
ifeq ($(profile),true)
  flags += -pg
  options += -pg
endif

# binaries built with this flag are faster, but are not portable to multiple machines.
ifeq ($(local),true)
  flags += -march=native
endif

# AngelScript internal profiler
ifeq ($(script_profiler),true)
  flags += -DAS_PROFILER_ENABLE -DAS_PROFILER_PERIOD_MICROSECONDS=101
endif

# the headless target builds the core without hiro (-DDISABLE_HIRO), which changes the layout of types every object
# shares, so it keeps its objects apart from the GUI build's:
ifeq ($(target),headless)
  object.path := obj-headless
endif

nall.path := ../nall
include $(nall.path)/GNUmakefile

ifeq ($(platform),windows)
  ifeq ($(binary),application)
    options += -luuid -lkernel32 -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32
    options += -Wl,-enable-auto-import
    options += -Wl,-enable-runtime-pseudo-reloc
  else ifeq ($(binary),library)
    options += -shared
  endif
else ifeq ($(platform),macos)
  ifeq ($(binary),application)
    # angelscript <= 2.36.1 native calling convention really only supports x86_64
    # angelscript >= 2.27.0 native calling convention now supports Apple Silicon M1,M2,M3, etc.
    # compiler flags
    #flags += -target x86_64-apple-macos10.12
    # linker options
    #options += -target x86_64-apple-macos10.12
  else ifeq ($(binary),library)
    flags += -fPIC
    options += -dynamiclib
  endif
else ifneq ($(filter $(platform),linux bsd),)
  ifeq ($(binary),application)
    options += -Wl,-export-dynamic
    options += -lX11 -lXext
  else ifeq ($(binary),library)
    flags += -fPIC
    options += -shared
  endif
endif

objects := libco emulator filter lzma script-platform script-profiler

$(object.path)/libco.o: ../libco/libco.c
$(object.path)/emulator.o: emulator/emulator.cpp
$(object.path)/filter.o: filter/filter.cpp
$(object.path)/lzma.o: lzma/lzma.cpp
$(object.path)/script-platform.o: script/script-platform.cpp
$(object.path)/script-profiler.o: script/script-profiler.cpp

include sfc/GNUmakefile
include gb/GNUmakefile
include processor/GNUmakefile

ui := target-$(target)
include $(ui)/GNUmakefile
-include $(object.path)/*.d

clean:
	$(call rdelete,$(object.path)/*)
	$(call rdelete,out/*)
