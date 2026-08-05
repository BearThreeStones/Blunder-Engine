# Physics golden reference dumps (kernel v0)

`win-msvc-x64.dump` is the **Windows MSVC x64 Debug** reference for all eight required golden scenarios. Recorded fields per body: pose (Q32.32 raw), linear velocity (Q32.32 raw), sleep flag.

## Regenerate reference (Windows MSVC x64)

```powershell
cmake --build build/vs2026-debug --config Debug --target physics_golden_suite
.\build\vs2026-debug\engine\src\tests\Debug\physics_golden_suite.exe `
  --dump engine\src\tests\fixtures\physics_golden\win-msvc-x64.dump
```

## Linux Clang or GCC x64 peer gate (tasks 4.10)

Linux CI must produce **bit-identical** state to this reference:

```bash
cmake --build build/linux-debug --target physics_golden_suite
./build/linux-debug/engine/src/tests/physics_golden_suite \
  --compare engine/src/tests/fixtures/physics_golden/win-msvc-x64.dump
```

Also run the default golden target (in-process Q32.32 asserts):

```bash
ctest -R physics_golden_suite
```

**Do not** check in Linux-generated dumps as authoritative until a Win/Linux dual-run confirms match. This environment records Win MSVC only.

## Dump format

Binary `BLDRPHYS` v1 — see `physics_golden_dump.cpp`.
