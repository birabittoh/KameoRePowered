#!/usr/bin/env python3
import os
import sys
import platform
import shutil
import subprocess
import tomllib


def detect_preset():
    os_name = platform.system()
    arch = platform.machine().lower()

    if os_name == "Linux":
        os_id = "linux"
    elif os_name == "Windows":
        os_id = "win"
    else:
        raise RuntimeError(f"Unsupported OS: {os_name}")

    if arch in ("x86_64", "amd64"):
        arch_id = "amd64"
    elif arch in ("aarch64", "arm64"):
        arch_id = "arm64"
    else:
        raise RuntimeError(f"Unsupported architecture: {arch}")

    return f"{os_id}-{arch_id}-release"


def check_deps():
    missing = [dep for dep in ("cmake", "ninja") if shutil.which(dep) is None]
    if missing:
        print(f"error: missing required tool(s): {', '.join(missing)}", file=sys.stderr)
        sys.exit(1)


def find_clang():
    # Versioned binaries (clang++-20, clang++-22, …) only exist on Linux.
    if platform.system() != "Windows":
        for version in range(30, 17, -1):
            if shutil.which(f"clang++-{version}"):
                return f"clang-{version}", f"clang++-{version}"
    if shutil.which("clang++"):
        return "clang", "clang++"
    print("error: no clang/clang++ compiler found in PATH", file=sys.stderr)
    sys.exit(1)


def run(args, **kwargs):
    print(f"+ {' '.join(str(a) for a in args)}")
    result = subprocess.run(args, **kwargs)
    if result.returncode != 0:
        sys.exit(result.returncode)


def load_manifest(path):
    with open(path, "rb") as f:
        return tomllib.load(f)


def copy_runtime_libs(is_windows):
    if is_windows:
        sdk_bin = os.path.join("sdk", "bin")
        if not os.path.isdir(sdk_bin):
            return
        for name in os.listdir(sdk_bin):
            if name.endswith(".dll") and not name.endswith("d.dll") and not name.endswith("rd.dll"):
                src = os.path.join(sdk_bin, name)
                print(f"+ cp {src} {name}")
                shutil.copy2(src, name)
    else:
        sdk_lib = os.path.join("sdk", "lib")
        for name in os.listdir(sdk_lib):
            if name.endswith(".so") and not name.endswith("d.so") and not name.endswith("rd.so"):
                src = os.path.join(sdk_lib, name)
                print(f"+ cp {src} {name}")
                shutil.copy2(src, name)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(script_dir, ".."))
    os.chdir(root)

    is_windows = platform.system() == "Windows"
    rexglue = os.path.join("sdk", "bin", "rexglue.exe" if is_windows else "rexglue")

    manifest_path = "kameorepowered_manifest.toml"
    manifest = load_manifest(manifest_path)
    project_name = manifest["project"]["name"]
    xex_path = manifest["entrypoint"]["file_path"]

    if not os.path.exists(xex_path):
        print(f"error: XEX not found at '{xex_path}' — place the game's default.xex there before building", file=sys.stderr)
        sys.exit(1)

    check_deps()

    preset = detect_preset()
    exe_name = f"{project_name}.exe" if is_windows else project_name
    build_output = os.path.join("out", "build", preset, exe_name)

    c_compiler, cxx_compiler = find_clang()

    run([rexglue, "codegen", manifest_path])
    run([
        "cmake", "--preset", preset,
        "-DCMAKE_PREFIX_PATH=sdk",
        f"-DCMAKE_C_COMPILER={c_compiler}",
        f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
    ])
    run(["cmake", "--build", "--preset", preset, "--parallel", str(os.cpu_count() or 1)])

    print(f"+ cp {build_output} {exe_name}")
    shutil.copy2(build_output, exe_name)

    copy_runtime_libs(is_windows)


if __name__ == "__main__":
    main()
