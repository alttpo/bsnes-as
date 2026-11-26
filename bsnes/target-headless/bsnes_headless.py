"""Drives bsnes-headless over its stdin/stdout protocol for script tests.

    from bsnes_headless import Headless

    with Headless("out/bsnes-headless", rom="alttp-us.sfc", script="../alttpo/alttpo/") as emu:
        emu.frame(400)
        emu.input(1, "start")
        emu.frame(5)
        emu.input(1)
        assert emu.read(0x7E0010, 1) == b"\\x02"
        print(emu.call("int count_players()"))
        print(emu.logs)          # every `log` line so far: (frame, level, text)
"""

import subprocess


class HeadlessError(Exception):
    pass


class Headless:
    def __init__(self, binary, rom, script=None, sram=None, extra_args=()):
        args = [binary]
        if script:
            args.append(f"--script={script}")
        if sram:
            args.append(f"--sram={sram}")
        args += list(extra_args)
        args.append(rom)
        self.proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.logs = []
        # the first answer is `ok ready <title>`, or `err` if the script failed to compile:
        self.title = self._answer().removeprefix("ready").strip()

    def _answer(self):
        while True:
            line = self.proc.stdout.readline()
            if not line:
                code = self.proc.wait()
                raise HeadlessError(f"bsnes-headless exited with code {code}; last logs: {self.logs[-10:]}")
            line = line.rstrip("\n")
            if line.startswith("log "):
                _, frame, level, text = (line.split(" ", 3) + [""])[:4]
                self.logs.append((int(frame), level, text))
            elif line == "ok" or line.startswith("ok "):
                return line[3:]
            elif line.startswith("err "):
                raise HeadlessError(line[4:])

    def command(self, line):
        self.proc.stdin.write(line + "\n")
        self.proc.stdin.flush()
        return self._answer()

    def frame(self, count=1):
        return int(self.command(f"frame {count}"))

    def input(self, port, *buttons):
        """Holds the named buttons (up down left right b a y x l r select start) until the next input call."""
        self.command(" ".join(["input", str(port), *buttons]))

    def read(self, addr, size=1):
        return bytes.fromhex(self.command(f"read ${addr:06x} {size}"))

    def write(self, addr, data):
        self.command(f"write ${addr:06x} {bytes(data).hex()}")

    def call(self, decl):
        return self.command(f"call {decl}")

    def exec(self, statements):
        self.command(f"exec {statements}")

    def save_state(self, path):
        self.command(f"save-state {path}")

    def load_state(self, path):
        self.command(f"load-state {path}")

    def screenshot(self, path):
        self.command(f"screenshot {path}")

    def errors(self):
        return [entry for entry in self.logs if entry[1] == "error"]

    def close(self):
        if self.proc.poll() is None:
            try:
                self.command("quit")
            except HeadlessError:
                pass
            self.proc.stdin.close()
            self.proc.wait()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
