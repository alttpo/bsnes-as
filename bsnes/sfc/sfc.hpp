#pragma once

//license: GPLv3
//started: 2004-10-14

#include <emulator/emulator.hpp>
#include <emulator/random.hpp>
#include <emulator/cheat.hpp>

#include <processor/arm7tdmi/arm7tdmi.hpp>
#include <processor/gsu/gsu.hpp>
#include <processor/hg51b/hg51b.hpp>
#include <processor/spc700/spc700.hpp>
#include <processor/upd96050/upd96050.hpp>
#include <processor/wdc65816/wdc65816.hpp>

extern "C" {
  #include <gb/Core/gb.h>
  #include <gb/Core/random.h>
}

namespace SuperFamicom {
  #define platform Emulator::platform
  namespace File = Emulator::File;
  using Random = Emulator::Random;
  using Cheat = Emulator::Cheat;
  extern Random random;
  extern Cheat cheat;

  struct Scheduler {
    enum class Mode : uint { Run, Synchronize } mode;
    enum class Event : uint { PreNMI, StartFrame, EndFrame, Synchronized, Desynchronized } event;

    cothread_t host = nullptr;
    cothread_t active = nullptr;
    bool desynchronized = false;

    auto enter() -> void {
      host = co_active();
      co_switch(active);
    }

    auto leave(Event event_) -> void {
      event = event_;
      active = co_active();
      co_switch(host);
    }

    auto resume(cothread_t thread) -> void {
      if(mode == Mode::Synchronize) desynchronized = true;
      co_switch(thread);
    }

    inline auto synchronizing() const -> bool {
      return mode == Mode::Synchronize;
    }

    inline auto synchronize() -> void {
      if(mode == Mode::Synchronize) {
        if(desynchronized) {
          desynchronized = false;
          leave(Event::Desynchronized);
        } else {
          leave(Event::Synchronized);
        }
      }
    }

    inline auto desynchronize() -> void {
      desynchronized = true;
    }
  };
  extern Scheduler scheduler;

  struct Thread {
    enum : uint { Size = 4_KiB * sizeof(void*) };

    auto create(auto (*entrypoint)() -> void, uint frequency_) -> void {
      if(!thread) {
        thread = co_create(Thread::Size, entrypoint);
      } else {
        thread = co_derive(thread, Thread::Size, entrypoint);
      }
      frequency = frequency_;
      clock = 0;
    }

    auto active() const -> bool {
      return thread == co_active();
    }

    auto serialize(serializer& s) -> void {
      s.integer(frequency);
      s.integer(clock);
    }

    auto serializeStack(serializer& s) -> void {
      static uint8_t stack[Thread::Size];
      bool active = co_active() == thread;

      if(s.mode() == serializer::Size) {
        s.array(stack, Thread::Size);
        s.boolean(active);
      }

      if(s.mode() == serializer::Load) {
        s.array(stack, Thread::Size);
        s.boolean(active);
        memory::copy(thread, stack, Thread::Size);
        if(active) scheduler.active = thread;
      }

      if(s.mode() == serializer::Save) {
        memory::copy(stack, thread, Thread::Size);
        s.array(stack, Thread::Size);
        s.boolean(active);
      }
    }

    cothread_t thread = nullptr;
      uint32_t frequency = 0;
       int64_t clock = 0;
  };

  struct Region {
    static inline auto NTSC() -> bool;
    static inline auto PAL() -> bool;
  };

  // used for scripts to read/write to PPU frame:
  struct PPUFrame {
    uint16_t *output = nullptr;
    uint pitch  = 512;
    uint width  = 512;
    uint height = 480;
    int  width_mult  = 2;
    int  height_mult = 2;
  };
  extern PPUFrame ppuFrame;

  namespace ScriptInterface {
    struct PPUAccess;
    struct GUI;
    struct PostFrame;
    namespace Net {
      struct Socket;
    }
  }

  struct Script {
#ifndef DISABLE_HIRO
    vector<hiro::Window> windows;
#endif

    vector<ScriptInterface::Net::Socket*> sockets;

    struct {
      asIScriptFunction *init = nullptr;
      asIScriptFunction *unload = nullptr;
      asIScriptFunction *post_power = nullptr;
      asIScriptFunction *cartridge_loaded = nullptr;
      asIScriptFunction *cartridge_unloaded = nullptr;
      asIScriptFunction *pre_nmi = nullptr;
      asIScriptFunction *pre_frame = nullptr;
      asIScriptFunction *post_frame = nullptr;
      asIScriptFunction *palette_updated = nullptr;
    } funcs;
  };
  extern Script script;

  #include <sfc/system/system.hpp>
  #include <sfc/memory/memory.hpp>
  #include <sfc/ppu/counter/counter.hpp>

  #include <sfc/cpu/cpu.hpp>
  #include <sfc/smp/smp.hpp>
  #include <sfc/dsp/dsp.hpp>
  #include <sfc/ppu/ppu.hpp>
  #include <sfc/ppu-fast/ppu.hpp>

  #include <sfc/controller/controller.hpp>
  #include <sfc/expansion/expansion.hpp>
  #include <sfc/coprocessor/coprocessor.hpp>
  #include <sfc/slot/slot.hpp>
  #include <sfc/cartridge/cartridge.hpp>

  #include <sfc/memory/memory-inline.hpp>
  #include <sfc/ppu/counter/counter-inline.hpp>
}

#include <sfc/interface/interface.hpp>
