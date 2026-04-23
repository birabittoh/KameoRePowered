# ReKameo

Static recompilation of **Kameo: Elements of Power** (Xbox 360) for Windows
and Linux, built on the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

This project converts the Xbox 360 PowerPC `default.xex` into native x86_64
code at build time, then wraps it with a small host runtime (logging,
overlays, hooks) so the game can be modded the same way PC ports are.

> **You must own the game.** ReKameo does **not** ship any Kameo code, data,
> or assets. You provide your own legally dumped `kameo.iso`. No pirate
> image of the game will be supported.

## Status

The recompilation pipeline is fully functional end-to-end:

- `rexglue codegen` produces ~110 MB of C++ across 15,204 recompiled
  functions with zero unresolved imports.
- The project builds to a native `rekameo.exe` (Windows) or `rekameo` (Linux).
- The runtime starts, initializes the renderer (D3D12/Vulkan), loads the XEX, and begins
  executing Kameo's guest code.
- The game currently crashes after ~1.3 seconds inside guest code — a
  null pointer dereference in Kameo's own startup logic. This is the
  active area of work.

What still needs manual reverse engineering work:

- Identifying and fixing the startup crash (add entries to
  `config/rekameo_ctx_ctr.toml` or hook the offending function).
- Populating `config/rekameo_hooked.toml` with useful intercept points
  (camera, save system, intro video skip, aspect ratio, ...).
- Populating `config/rekameo_midasm.toml` with inline patches.
- A custom renderer to replace the ReXGlue-provided D3D translation layer.

## Prerequisites

- **OS**: Windows 10/11 or Linux (x86_64)
- **Clang**: Version 20+ recommended
- **CMake + Ninja**: Modern versions (3.25+)
- **ReXGlue SDK**: Built from source (requires the same commit for codegen)
- **extract-xiso**: Used to extract `kameo.iso`

### Linux (Arch/CachyOS)
```bash
paru -S clang cmake ninja vulkan-headers extract-xiso-git
```

### Windows
- Install Clang via [scoop](https://scoop.sh): `scoop install llvm`
- Install CMake/Ninja (included with VS 2022 Build Tools)
- Drop `extract-xiso.exe` and `xextool.exe` in `tools\`

## Building from scratch

### 1. Build the ReXGlue SDK

```bat
git clone --recursive https://github.com/rexglue/rexglue-sdk.git
cd rexglue-sdk
cmake --preset win-amd64 -DCMAKE_CXX_FLAGS="-march=x86-64-v3" -DCMAKE_C_FLAGS="-march=x86-64-v3"
cmake --build --preset win-amd64-relwithdebinfo --target install
```

> **Important:** use the `rexglue.exe` produced by this build (at
> `out\win-amd64\rexglue.exe`) for codegen — not a separately downloaded
> binary. The generated C++ must match the SDK version it links against.

Set the environment variable `REXSDK_DIR` to the cloned SDK root, or install it and set `CMAKE_PREFIX_PATH` to the install directory.

### 2. Clone ReKameo

```bat
git clone https://github.com/<you>/ReKameo.git
cd ReKameo
```

### 3. Provide your game

Drop your legally dumped Xbox 360 Kameo image at `kameo.iso` in the
repo root.

### 4. Extract the XEX

**Windows**:
```bat
WindowsScripts\Setup.bat
```

**Linux**:
```bash
mkdir -p out/iso_extract assets
extract-xiso -d out/iso_extract kameo.iso
cp out/iso_extract/default.xex assets/default.xex
```

Requires `extract-xiso.exe` in `tools\` or on `PATH`. Drops
`assets\default.xex` ready for codegen.

### 5. Run codegen

**Windows**:
```bat
path\to\rexglue-sdk\out\win-amd64\rexglue.exe codegen rekameo_config.toml
```

**Linux**:
```bash
# First time or after SDK update:
path/to/rexglue-sdk/out/linux-amd64/RelWithDebInfo/rexglue migrate rekameo_config.toml --app_root .

# Then run codegen:
path/to/rexglue-sdk/out/linux-amd64/RelWithDebInfo/rexglue codegen rekameo_config.toml
```

### 6. Build

**Windows**:
```bat
cmake --preset win-amd64-relwithdebinfo -DREXSDK_DIR="%REXSDK_DIR%"
cmake --build --preset win-amd64-relwithdebinfo
```

**Linux**:
```bash
cmake --preset linux-amd64-relwithdebinfo -DCMAKE_PREFIX_PATH="/path/to/sdk/install"
cmake --build --preset linux-amd64-relwithdebinfo
```

### 7. Run

**Windows**:
```bat
WindowsScripts\Play.bat
```

**Linux**:
```bash
./out/build/linux-amd64-relwithdebinfo/rekameo
```

`PlayConsole.bat` adds `--enable_console=true` and writes a log to
`logs\rekameo.log`. `PlayDebug.bat` opens the exe in Visual Studio for
debugging.

## XEX overview (Kameo: Elements of Power, NTSC)

| Field | Value |
|---|---|
| Title ID | `4D5307D2` (MS-2002) |
| Load base | `0x82000000` |
| Entry point | `0x8251F320` |
| Image size | `0x00BF0000` (~12 MB loaded) |
| Build date | 17 Oct 2005 |
| Static libs | D3DX9 / XGRAPHC / XAPILIB / XRTLLIB / XONLINE / LIBCMT / XBOXKRNL / XHV / XMP / D3D9LTCG / XAUDLTCG / X3DAUDL / XACTLTCG (XDK 2099) |
| Imports | `xboxkrnl.exe` (148 functions, 8 data), `xam.xex` (65 functions, 2 data) |
| Decoded instructions | 1,621,549 across 2,141 code regions |
| Discovered functions | 15,204 (after call-graph, PDATA, vtable, gap-fill) |

## Analysis tooling

Host-side scripts live in `tools/` (gitignored binaries, tracked scripts).

```bat
:: Unpack ISO → assets\default.xex
WindowsScripts\Setup.bat

:: Resolve all kernel/xam import stubs to named symbols → out\kameo_imports.csv
powershell -NoProfile -File tools\resolve_imports.ps1

:: Peek at raw PPC instructions around a guest address
powershell -NoProfile -File tools\peek_ppc.ps1 -Address 0x8251F320
```

`tools/resolve_imports.ps1` cross-references `out/default.xex.idc`
(from `xextool -i`) with `tools/x360_imports.idc` to produce a full
address-to-name CSV of every kernel/xam import.

## Repo layout

```
ReKameo/
├── assets/                       ← default.xex lands here (gitignored)
├── config/                       ← codegen TOMLs: CRT, hooks, midasm, etc.
├── generated/                    ← produced by rexglue codegen (gitignored)
├── icon/                         ← Windows icon + .rc resource
├── src/                          ← hand-written host-side runtime
│   ├── main.cpp
│   ├── rekameo_app.h
│   └── kameo_engine/
│       ├── hooks.{h,cpp}
│       ├── Log.h
│       ├── rex_macros.h
│       └── Overlays/Fps.{h,cpp}
├── tools/                        ← RE scripts (tracked); binaries gitignored
├── WindowsScripts/               ← Setup, Migrate, Rebuild, Play, Debug
├── .github/                      ← CI + issue templates
├── CMakeLists.txt
├── CMakePresets.json
├── rekameo_config.toml           ← top-level codegen config
└── rekameo.toml                  ← runtime cvar overrides (fullscreen, vsync, …)
```

## Adding a hook

1. Find the guest address of the function in Kameo's `default.xex`.
2. Add it to `config/rekameo_hooked.toml`:

   ```toml
   0x8XXXXXXX = {name = "rex_myFunction_8XXXXXXX"}
   ```

3. Implement the hook in `src/kameo_engine/hooks.cpp`:

   ```cpp
   int myFunction_8XXXXXXX_Hook(/* args */) {
       // your logic
       return 0;
   }
   REX_PPC_HOOK(myFunction_8XXXXXXX);
   ```

4. Re-run `WindowsScripts\RegenAndRebuild.bat`.

## Adding a crash fix

If a guest function crashes (null deref, bad memory access), add its
address to `config/rekameo_ctx_ctr.toml` to preserve full PPC context
around the call, then hook it to skip or stub the bad path:

```toml
# config/rekameo_ctx_ctr.toml
0x8XXXXXXX = {}   # describe what this guards
```

## Known issues

- **Startup crash (~1.3s in):** Kameo's guest code hits a null pointer
  during early initialization. Root cause under investigation. Likely a
  missing stub or an uninitialized subsystem that needs a hook.
- **No window on some systems:** If D3D12 device creation fails silently,
  set `fullscreen = false` in `rekameo.toml` and retry.
- **CI uses rexglue 0.7.4:** The GitHub Actions workflow downloads the
  pinned 0.7.4 release. Local builds use the SDK source directly via
  `REXSDK_DIR`. These may diverge; update the workflow URL when a
  compatible release is published.

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) — the recompilation framework ReKameo is built on.
- TiP-Recomp — build scaffolding and workflow pattern used as a starting point.

## License

The host-side source in `src/`, build scripts, and CI config are
available under the MIT License (add a `LICENSE` file before publishing).

The recompiled game code produced at build time contains symbols and
logic from Kameo: Elements of Power and is **not** redistributable. Do
not share `default.xex`, the `generated/` directory, or any built
`rekameo.exe` that links against them.
