# Kameo: RePowered

Static recompilation of **Kameo: Elements of Power** (Xbox 360) for Windows
and Linux, built on the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

This project converts the Xbox 360 PowerPC `default.xex` into native x86_64
code at build time, then wraps it with a small host runtime (logging,
overlays, hooks) so the game runs natively and can be modded like a PC port.

> **You must own the game.** This project does **not** ship any Kameo code,
> data, or assets. You provide your own legally dumped `kameo.iso`.

## Using a pre-built release

Get the latest stable build [here](https://github.com/MaxDeadBear/KameoRePowered/releases/latest).

Nightly builds are available [here](https://nightly.link/MaxDeadBear/KameoRePowered/workflows/ci/main?preview).

Just place the downloaded executable next to the extracted `assets` directory and run it.

## Building from scratch

### 0. Install dependencies

#### Linux (Arch/CachyOS)
```bash
paru -S clang20 cmake ninja vulkan-headers extract-xiso
```

#### Windows
```powershell
scoop install llvm cmake ninja
```

### 1. Clone

```bash
git clone https://github.com/MaxDeadBear/KameoRePowered.git
cd KameoRePowered
```

### 2. Download the ReXGlue SDK

```bash
python scripts/download-sdk.py
```

This downloads the latest nightly and installs it into `sdk/<platform>/`.

### 3. Provide your game

Extract your legally dumped ISO directly into `assets/`:

```bash
extract-xiso -d assets "Kameo - Elements of Power (USA).iso"
```

`assets/default.xex` must exist before running codegen.

### 4. Run codegen

```bash
sdk/linux-amd64/bin/rexglue codegen kameorepowered_config.toml
```

```powershell
.\sdk\win-amd64\bin\rexglue.exe codegen .\kameorepowered_config.toml
```

### 5. Run migrate (generates `generated/rexglue.cmake`)

```bash
sdk/linux-amd64/bin/rexglue migrate --app_root .
```

```powershell
.\sdk\win-amd64\bin\rexglue.exe migrate --app_root .
```

### 6. Build

```bash
cmake --preset linux-amd64-release -DCMAKE_PREFIX_PATH="sdk/linux-amd64"
cmake --build --preset linux-amd64-release -- -j$(nproc)
```

```powershell
cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH="sdk/win-amd64"
cmake --build --preset win-amd64-release -- -j $env:NUMBER_OF_PROCESSORS
```

Symlink assets into the build output so the binary can find them:

```bash
ln -sf "$PWD/assets" out/build/linux-amd64-release/assets
```

```powershell
New-Item -ItemType Junction -Path out/build/win-amd64-release/assets -Target "$PWD/assets"
```

## Options

Options can be persisted by adding them to `kameorepowered.toml` next to the game executable, for example:

```toml
vulkan_device = 1 # NVIDIA GPU
user_language = 4 # French
```

### Language selection

The game defaults to English. Pass `--user_language <id>` to switch:

| ID | Language   |
|----|------------|
| 1  | English    |
| 2  | Japanese   |
| 3  | German     |
| 4  | French     |
| 5  | Spanish    |
| 6  | Italian    |
| 7  | Korean     |

```bash
./kameorepowered --user_language 6
```

### GPU selection

If you have multiple GPUs, you can force a specific one:

```bash
./kameorepowered --vulkan_device 1
```

List available devices by running the game without the flag.

### Logging

The game writes logs into the `logs` directory by default, but you can configure it.

```bash
./kameorepowered --log_file kameo.log --log_level debug
```

## Adding a hook

1. Find the guest address in `default.xex`.
2. Add to `kameorepowered_config.toml`:

   ```toml
   [functions]
   0x8XXXXXXX = {name = "MyFunction"}
   ```

3. Implement in `src/kameorepowered_hooks.cpp`:

   ```cpp
   void MyFunction(PPCContext& ctx, uint8_t* base) {
       // your logic
   }
   ```

4. Re-run codegen and rebuild.

## Adding a midasm hook (inline patch)

```toml
[[midasm_hook]]
address = 0x8XXXXXXX
name = "MyHook"
registers = ["r3"]
return = true
```

Implement in `src/kameorepowered_hooks.cpp`:

```cpp
void MyHook(PPCRegister& r3) {
    r3.u32 = 1;
}
```

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)
- [MaxDeadBear](https://github.com/MaxDeadBear)
- [BiRabittoh](https://github.com/birabittoh)

## License

The host-side source in `src/`, build scripts, and CI config are available
under the MIT License.

The recompiled game code produced at build time contains symbols and logic
from Kameo: Elements of Power and is **not** redistributable. Do not share
`default.xex`, the `generated/` directory, or any built binary that links
against them.
