processors += wdc65816 spc700 arm7tdmi

objects += sfc-interface sfc-system sfc-controller
objects += sfc-cartridge sfc-memory
objects += sfc-cpu sfc-smp sfc-dsp sfc-ppu sfc-ppu-fast
objects += sfc-expansion sfc-coprocessor sfc-slot
objects += sfc-resource

$(object.path)/sfc-interface.o:   sfc/interface/interface.cpp
$(object.path)/sfc-resource.o:    sfc/resource/resource.cpp
$(object.path)/sfc-system.o:      sfc/system/system.cpp
$(object.path)/sfc-controller.o:  sfc/controller/controller.cpp
$(object.path)/sfc-cartridge.o:   sfc/cartridge/cartridge.cpp
$(object.path)/sfc-memory.o:      sfc/memory/memory.cpp

$(object.path)/sfc-cpu.o:         sfc/cpu/cpu.cpp
$(object.path)/sfc-smp.o:         sfc/smp/smp.cpp
$(object.path)/sfc-dsp.o:         sfc/dsp/dsp.cpp
$(object.path)/sfc-ppu.o:         sfc/ppu/ppu.cpp
$(object.path)/sfc-ppu-fast.o:    sfc/ppu-fast/ppu.cpp

$(object.path)/sfc-expansion.o:   sfc/expansion/expansion.cpp
$(object.path)/sfc-coprocessor.o: sfc/coprocessor/coprocessor.cpp
$(object.path)/sfc-slot.o:        sfc/slot/slot.cpp
