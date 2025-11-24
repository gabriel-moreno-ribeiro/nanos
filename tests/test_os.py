#!/usr/bin/env python3
"""Boots nanos in QEMU with the serial console attached to a pipe, drives the
shell with commands and checks the answers. Run with `make test`."""
import os
import re
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE = os.path.join(ROOT, "build", "os.img")

passed = 0
failed = 0


def check(name, condition, detail=""):
    global passed, failed
    if condition:
        passed += 1
    else:
        failed += 1
        print(f"FAIL: {name} {detail}")


def boot(commands, timeout=40):
    """Runs the image, typing each command with a short pause; returns (output, exit code)."""
    if not os.path.exists(IMAGE):
        subprocess.run(["make", "-C", ROOT], check=True)
    proc = subprocess.Popen(
        ["qemu-system-i386", "-drive", f"format=raw,file={IMAGE}", "-display", "none", "-serial", "stdio",
         "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04", "-no-reboot"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    script = "".join(c + "\r" for c in commands).encode()
    try:
        # give the kernel a moment to boot, then feed the script slowly enough for the polling shell
        time.sleep(1.5)
        for chunk in script:
            proc.stdin.write(bytes([chunk]))
            proc.stdin.flush()
            time.sleep(0.003)
        out, _ = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
        return out.decode("latin-1"), None
    return out.decode("latin-1"), proc.returncode


def main():
    subprocess.run(["make", "-C", ROOT], check=True)
    out, code = boot(["help", "echo hello nanos", "mem", "alloc 100", "uptime", "spawn one", "spawn two", "ps",
                      "int3", "div0", "echo still alive", "bogus", "uptime", "ps", "exit"])
    print(out)
    check("qemu exited through the debug port", code == 1, str(code))
    check("boot banner", "nanos 1.0" in out)
    check("memory selftest", "selftest: memory ok" in out)
    check("help lists commands", "commands:" in out and "spawn" in out)
    check("echo", "\nhello nanos" in out.replace("\r", ""))
    m = re.search(r"frames: (\d+) free of (\d+)", out)
    check("mem reports frames", m is not None and int(m.group(1)) == int(m.group(2)) == 3584, m.group(0) if m else "")
    check("alloc touches and frees frames", "allocated 100 frames" in out and "freed them, 3584 free" in out)
    check("alloc reduces free count while held", re.search(r"allocated 100 frames, first at 200000, 3484 free now", out) is not None)
    ticks = [int(t) for t in re.findall(r"(\d+) ticks \(", out)]
    check("timer ticks advance", len(ticks) == 2 and ticks[1] > ticks[0] > 0, str(ticks))
    check("tasks spawned", "spawned task 2 (one)" in out and "spawned task 3 (two)" in out)
    check("tasks finish", "[task one done]" in out and "[task two done]" in out)
    last_ps = out.rsplit("id  state", 1)[-1]
    check("ps shows finished tasks", "finished" in last_ps and re.search(r"one\s*\n", last_ps) is not None)
    check("counters ran to completion", "counters: 200000 200000" in last_ps)
    check("kernel task keeps switching", re.search(r"1\s+runnable\s+(\d+)\s+kernel", last_ps) and int(re.search(r"1\s+runnable\s+(\d+)\s+kernel", last_ps).group(1)) > 0)
    check("breakpoint exception is reported", re.search(r"exception 3 \(breakpoint\) at eip=1[0-9a-f]{4}", out) is not None)
    check("divide error is survived", "exception 0 (divide error)" in out and "still alive" in out)
    check("unknown command", "unknown command: bogus" in out)
    check("clean exit", "bye" in out)
    print(f"{passed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
