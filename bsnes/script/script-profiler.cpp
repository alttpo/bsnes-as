
#include <script/script.hpp>

namespace Script {

#if !defined(AS_PROFILER_PERIOD_MICROSECONDS)
#  define AS_PROFILER_PERIOD_MICROSECONDS 101
#endif

Profiler::~Profiler() {
  enabled = false;
  thrProfiler.join();
}

Profiler::node_t::node_t(const string &section, int line) : section(section), line(line) {}
Profiler::node_t::node_t(const string &section, int line, uint64 samples) : section(section), line(line), samples(samples) {}

auto Profiler::node_t::operator<(const node_t &source) const -> bool {
  int c = strcmp(section, source.section);
  if (c < 0) return true;
  if (c > 0) return false;
  return line < source.line;
}

auto Profiler::node_t::operator==(const node_t &source) const -> bool {
  int c = section.compare(source.section);
  if (c != 0) return false;
  return line == source.line;
}

auto Profiler::samplingThread(uintptr p) -> void {
  while (enabled) {
    // sleep until next period (in microseconds):
    usleep(AS_PROFILER_PERIOD_MICROSECONDS);

    // wake up and record last sampled script location:
    recordSample();

    // auto-save every 5 seconds:
    auto time = chrono::microsecond();
    if (time - last_save >= 5'000'000) {
      save();
      last_save = time;
    }
  }
}

auto Profiler::lineCallback(asIScriptContext *ctx) -> void {
  sampleLocation(ctx);
}

auto Profiler::enable(asIScriptContext *ctx) -> void {
  mtx.lock();

  enabled = true;
  thrProfiler = nall::thread::create({&Profiler::samplingThread, this});
  //ctx->SetLineCallback(asMETHOD(ScriptInterface::Profiler, lineCallback), this, asCALL_THISCALL);
  ctx->SetLineCallback(
    asFUNCTION(+([](asIScriptContext *ctx, Profiler &self) {
      self.lineCallback(ctx);
    })),
    this,
    asCALL_CDECL
  );

  mtx.unlock();
}

auto Profiler::disable(asIScriptContext *ctx) -> void {
  mtx.lock();

  ctx->ClearLineCallback();
  enabled = false;

  mtx.unlock();
}

void Profiler::reset() {
  sectionLineSamples.reset();
}

void Profiler::save() {
  mtx.lock();

  auto fb = file_buffer({"perf-", getpid(), ".csv"}, file_buffer::mode::write);
  fb.truncate(0);
  fb.writes({"section,line,samples\n"});
  for (auto &node : sectionLineSamples) {
    fb.writes({node.section, ",", node.line, ",", node.samples, "\n"});
  }

  mtx.unlock();
}

auto Profiler::sampleLocation(asIScriptContext *ctx) -> void {
  mtx.lock();

  // sample where we are:
  const char *section;
  line = ctx->GetLineNumber(0, &column, &section);

  if (section == nullptr) {
    scriptSection = "<not in script>";
    line = 0;
  } else {
    scriptSection = section;
  }

  mtx.unlock();
}

auto Profiler::resetLocation() -> void {
  mtx.lock();

  scriptSection = "<not in script>";
  line = 0;
  column = 0;

  mtx.unlock();
}

auto Profiler::recordSample() -> void {
  mtx.lock();

  // increment sample counter for the section/line:
  auto node = sectionLineSamples.find({scriptSection, line});
  if (!node) {
    sectionLineSamples.insert({{scriptSection}, line, 1});
  } else {
    node->samples++;
  }

  mtx.unlock();
}

}
