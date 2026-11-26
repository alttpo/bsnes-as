flags += -DGB_INTERNAL -DDISABLE_DEBUGGER -D_GNU_SOURCE -Wno-multichar
options += -I../sameboy

objects += gb-apu gb-camera gb-display gb-gb gb-joypad gb-mbc
objects += gb-memory gb-printer gb-random gb-rewind gb-save_state gb-sgb
objects += gb-sm83_cpu gb-symbol_hash gb-timing
#objects+= gb-debugger gb-sm83_disassembler

$(object.path)/gb-apu.o:               gb/Core/apu.c
$(object.path)/gb-camera.o:            gb/Core/camera.c
$(object.path)/gb-debugger.o:          gb/Core/debugger.c
$(object.path)/gb-display.o:           gb/Core/display.c
$(object.path)/gb-gb.o:                gb/Core/gb.c
$(object.path)/gb-joypad.o:            gb/Core/joypad.c
$(object.path)/gb-mbc.o:               gb/Core/mbc.c
$(object.path)/gb-memory.o:            gb/Core/memory.c
$(object.path)/gb-printer.o:           gb/Core/printer.c
$(object.path)/gb-random.o:            gb/Core/random.c
$(object.path)/gb-rewind.o:            gb/Core/rewind.c
$(object.path)/gb-save_state.o:        gb/Core/save_state.c
$(object.path)/gb-sgb.o:               gb/Core/sgb.c
$(object.path)/gb-sm83_cpu.o:          gb/Core/sm83_cpu.c
$(object.path)/gb-sm83_disassembler.o: gb/Core/sm83_disassembler.c
$(object.path)/gb-symbol_hash.o:       gb/Core/symbol_hash.c
$(object.path)/gb-timing.o:            gb/Core/timing.c
