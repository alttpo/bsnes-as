#include <sfc/sfc.hpp>

namespace SuperFamicom {

System system;
Scheduler scheduler;
Random random;
Cheat cheat;
Script script;
#include "serialization.cpp"

struct ALTTP {
    uint32_t room = 0;
    int16_t xoffs = 0, yoffs = 0;
    int16_t sprx[16] = {0};
    int16_t spry[16] = {0};
    uint8_t sprt[16] = {0};
    uint8_t sprs[16] = {0};
} alttp;

auto System::postRender(uint16_t *data, uint pitch, uint width, uint height) -> void {
  auto plot = [&](int x, int y, uint16_t color) -> void {
    if (x >= 0 && y >= 0 && x < (int) width && y < (int) height) {
      data[y * pitch + x] = color;
    }
  };

  auto hline = [&](int lx, int ty, int w, uint16_t color) {
    for (int x = lx; x < lx + w; ++x)
      plot(x, ty, color);
  };

  auto vline = [&](int lx, int ty, int h, uint16_t color) {
    for (int y = ty; y < ty + h; ++y)
      plot(lx, y, color);
  };

  for (auto i : range(16)) {
    if (alttp.sprt[i] != 0 && alttp.sprs[i] != 0) {
      auto rx = (int16_t)alttp.sprx[i] - (int16_t)alttp.xoffs;
      auto ry = (int16_t)alttp.spry[i] - (int16_t)alttp.yoffs;
      hline(rx     , ry     , 16, 0x7fff);
      vline(rx     , ry     , 16, 0x7fff);
      hline(rx     , ry + 15, 16, 0x7fff);
      vline(rx + 15, ry     , 16, 0x7fff);
      //printf("[%x] x=%04x,y=%04x t=%02x\n", i, x, y, t);
    }
  }
}

auto System::run() -> void {
  scheduler.mode = Scheduler::Mode::Run;
  scheduler.enter();
  if(scheduler.event == Scheduler::Event::Frame) {
    ppu.refresh();

    //refresh all cheat codes once per frame
    Memory::GlobalWriteEnable = true;
    for(auto& code : cheat.codes) {
      if(code.enable) {
        bus.write(code.address, code.data);
      }
    }
    Memory::GlobalWriteEnable = false;

    script.context->Prepare(script.funcs.main);

    if (script.funcs.main) {
      script.context->Prepare(script.funcs.main);
      script.context->Execute();
    }

#if 0
    if (system.hacks.alttp) {
      auto in_dark_world = bus.read(0x0FFF);
      auto in_dungeon = bus.read(0x001B);
      auto room_overworld = bus.read(0x008A) | (bus.read(0x008B) << 8u);
      auto room_dungeon = bus.read(0x00A0) | (bus.read(0x00A1) << 8u);

      alttp.room = ((in_dark_world & 1) << 18) | ((in_dungeon & 1) << 17) | (in_dungeon ? room_dungeon : room_overworld);

      alttp.xoffs = bus.read(0x00E2) | (bus.read(0x00E3) << 8u);
      alttp.yoffs = bus.read(0x00E8) | (bus.read(0x00E9) << 8u);

      for (auto i : range(16)) {
	alttp.spry[i] = bus.read(0x0D00u + i) | (bus.read(0x0D20u + i) << 8u);
	alttp.sprx[i] = bus.read(0x0D10u + i) | (bus.read(0x0D30u + i) << 8u);
	alttp.sprt[i] = bus.read(0x0E20u + i);
	alttp.sprs[i] = bus.read(0x0DD0u + i);
      }
    }
#endif
  }
}

auto System::runToSave() -> void {
  scheduler.mode = Scheduler::Mode::SynchronizeCPU;
  while(true) { scheduler.enter(); if(scheduler.event == Scheduler::Event::Synchronize) break; }

  scheduler.mode = Scheduler::Mode::SynchronizeAll;
  scheduler.active = smp.thread;
  while(true) { scheduler.enter(); if(scheduler.event == Scheduler::Event::Synchronize) break; }

  scheduler.mode = Scheduler::Mode::SynchronizeAll;
  scheduler.active = ppu.thread;
  while(true) { scheduler.enter(); if(scheduler.event == Scheduler::Event::Synchronize) break; }

  for(auto coprocessor : cpu.coprocessors) {
    scheduler.mode = Scheduler::Mode::SynchronizeAll;
    scheduler.active = coprocessor->thread;
    while(true) { scheduler.enter(); if(scheduler.event == Scheduler::Event::Synchronize) break; }
  }
}

auto System::load(Emulator::Interface* interface) -> bool {
  information = {};

  bus.reset();
  if(!cpu.load()) return false;
  if(!smp.load()) return false;
  if(!ppu.load()) return false;
  if(!dsp.load()) return false;
  if(!cartridge.load()) return false;

  if(cartridge.region() == "NTSC") {
    information.region = Region::NTSC;
    information.cpuFrequency = Emulator::Constants::Colorburst::NTSC * 6.0;
  }
  if(cartridge.region() == "PAL") {
    information.region = Region::PAL;
    information.cpuFrequency = Emulator::Constants::Colorburst::PAL * 4.8;
  }

  if(cartridge.has.ICD) {
    if(!icd.load()) return false;
  }
  if(cartridge.has.BSMemorySlot) bsmemory.load();

  serializeInit();
  this->interface = interface;
  return information.loaded = true;
}

auto System::save() -> void {
  if(!loaded()) return;

  cartridge.save();
}

auto System::unload() -> void {
  if(!loaded()) return;

  controllerPort1.unload();
  controllerPort2.unload();
  expansionPort.unload();

  if(cartridge.has.ICD) icd.unload();
  if(cartridge.has.MCC) mcc.unload();
  if(cartridge.has.Event) event.unload();
  if(cartridge.has.SA1) sa1.unload();
  if(cartridge.has.SuperFX) superfx.unload();
  if(cartridge.has.HitachiDSP) hitachidsp.unload();
  if(cartridge.has.SPC7110) spc7110.unload();
  if(cartridge.has.SDD1) sdd1.unload();
  if(cartridge.has.OBC1) obc1.unload();
  if(cartridge.has.MSU1) msu1.unload();
  if(cartridge.has.BSMemorySlot) bsmemory.unload();
  if(cartridge.has.SufamiTurboSlotA) sufamiturboA.unload();
  if(cartridge.has.SufamiTurboSlotB) sufamiturboB.unload();

  cartridge.unload();
  information.loaded = false;
}

auto System::power(bool reset) -> void {
  hacks.fastPPU = configuration.hacks.ppu.fast;

  Emulator::audio.reset(interface);

  random.entropy(Random::Entropy::Low);  //fallback case
  if(configuration.hacks.entropy == "None") random.entropy(Random::Entropy::None);
  if(configuration.hacks.entropy == "Low" ) random.entropy(Random::Entropy::Low );
  if(configuration.hacks.entropy == "High") random.entropy(Random::Entropy::High);

  ppufast.postRender = {&System::postRender, this};

  cpu.power(reset);
  smp.power(reset);
  dsp.power(reset);
  ppu.power(reset);

  if(cartridge.has.ICD) icd.power();
  if(cartridge.has.MCC) mcc.power();
  if(cartridge.has.DIP) dip.power();
  if(cartridge.has.Event) event.power();
  if(cartridge.has.SA1) sa1.power();
  if(cartridge.has.SuperFX) superfx.power();
  if(cartridge.has.ARMDSP) armdsp.power();
  if(cartridge.has.HitachiDSP) hitachidsp.power();
  if(cartridge.has.NECDSP) necdsp.power();
  if(cartridge.has.EpsonRTC) epsonrtc.power();
  if(cartridge.has.SharpRTC) sharprtc.power();
  if(cartridge.has.SPC7110) spc7110.power();
  if(cartridge.has.SDD1) sdd1.power();
  if(cartridge.has.OBC1) obc1.power();
  if(cartridge.has.MSU1) msu1.power();
  if(cartridge.has.Cx4) cx4.power();
  if(cartridge.has.DSP1) dsp1.power();
  if(cartridge.has.DSP2) dsp2.power();
  if(cartridge.has.DSP4) dsp4.power();
  if(cartridge.has.ST0010) st0010.power();
  if(cartridge.has.BSMemorySlot) bsmemory.power();
  if(cartridge.has.SufamiTurboSlotA) sufamiturboA.power();
  if(cartridge.has.SufamiTurboSlotB) sufamiturboB.power();

  if(cartridge.has.ICD) cpu.coprocessors.append(&icd);
  if(cartridge.has.Event) cpu.coprocessors.append(&event);
  if(cartridge.has.SA1) cpu.coprocessors.append(&sa1);
  if(cartridge.has.SuperFX) cpu.coprocessors.append(&superfx);
  if(cartridge.has.ARMDSP) cpu.coprocessors.append(&armdsp);
  if(cartridge.has.HitachiDSP) cpu.coprocessors.append(&hitachidsp);
  if(cartridge.has.NECDSP) cpu.coprocessors.append(&necdsp);
  if(cartridge.has.EpsonRTC) cpu.coprocessors.append(&epsonrtc);
  if(cartridge.has.SharpRTC) cpu.coprocessors.append(&sharprtc);
  if(cartridge.has.SPC7110) cpu.coprocessors.append(&spc7110);
  if(cartridge.has.MSU1) cpu.coprocessors.append(&msu1);
  if(cartridge.has.BSMemorySlot) cpu.coprocessors.append(&bsmemory);

  scheduler.active = cpu.thread;

  controllerPort1.power(ID::Port::Controller1);
  controllerPort2.power(ID::Port::Controller2);
  expansionPort.power();

  controllerPort1.connect(settings.controllerPort1);
  controllerPort2.connect(settings.controllerPort2);
  expansionPort.connect(settings.expansionPort);
}

}
