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


def copy_runtime_libs(is_windows, sdk_dir):
    if is_windows:
        src_dir = os.path.join(sdk_dir, "bin")
        suffix, bad = ".dll", ("d.dll", "rd.dll")
    else:
        src_dir = os.path.join(sdk_dir, "lib")
        suffix, bad = ".so", ("d.so", "rd.so")

    if not os.path.isdir(src_dir):
        return
    for name in os.listdir(src_dir):
        if name.endswith(suffix) and not any(name.endswith(b) for b in bad):
            src = os.path.join(src_dir, name)
            print(f"+ cp {src} {name}")
            shutil.copy2(src, name)


def do_package(name, project_name, is_windows):
    import zipfile
    import tarfile

    pkg_dir = "pkg"
    os.makedirs(pkg_dir, exist_ok=True)

    exe = f"{project_name}.exe" if is_windows else project_name
    lib_suffix = ".dll" if is_windows else ".so"
    candidates = [exe] + sorted(f for f in os.listdir(".") if f.endswith(lib_suffix))
    for src in candidates:
        if os.path.isfile(src):
            print(f"+ cp {src} {pkg_dir}/")
            shutil.copy2(src, pkg_dir)

    if is_windows:
        archive_path = f"{name}.zip"
        print(f"+ zip {archive_path}")
        with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for f in sorted(os.listdir(pkg_dir)):
                zf.write(os.path.join(pkg_dir, f), f)
    else:
        archive_path = f"{name}.tar.gz"
        print(f"+ tar {archive_path}")
        with tarfile.open(archive_path, "w:gz") as tf:
            for f in sorted(os.listdir(pkg_dir)):
                tf.add(os.path.join(pkg_dir, f), arcname=f)

    github_env = os.environ.get("GITHUB_ENV")
    if github_env:
        with open(github_env, "a") as fh:
            fh.write(f"ARTIFACT_PATH={archive_path}\n")
            fh.write(f"ARTIFACT_NAME={name}\n")


def parse_args():
    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("--sdk-dir", default="sdk", help="Path to the ReXGlue SDK (default: sdk)")
    p.add_argument("--package", metavar="NAME", help="Package built output into NAME.zip (Windows) or NAME.tar.gz (Linux); skips the build")
    return p.parse_args()


def main():
    args = parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(script_dir, ".."))
    os.chdir(root)

    is_windows = platform.system() == "Windows"

    manifest_path = "kameorepowered_manifest.toml"
    manifest = load_manifest(manifest_path)
    project_name = manifest["project"]["name"]

    if args.package:
        do_package(args.package, project_name, is_windows)
        return

    sdk_dir = args.sdk_dir
    rexglue = os.path.join(sdk_dir, "bin", "rexglue.exe" if is_windows else "rexglue")
    xex_path = manifest["entrypoint"]["file_path"]

    if not os.path.exists(xex_path):
        print(f"error: XEX not found at '{xex_path}' — place the game's default.xex there before building", file=sys.stderr)
        sys.exit(1)

    check_deps()

    preset = detect_preset()
    exe_name = f"{project_name}.exe" if is_windows else project_name
    build_output = os.path.join("out", "build", preset, exe_name)

    c_compiler, cxx_compiler = find_clang()

    cmake_configure_args = [
        f"-DCMAKE_PREFIX_PATH={sdk_dir}",
        f"-DCMAKE_C_COMPILER={c_compiler}",
        f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
    ]
    if shutil.which("sccache"):
        cmake_configure_args += [
            "-DCMAKE_C_COMPILER_LAUNCHER=sccache",
            "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache",
        ]

    run([rexglue, "codegen", manifest_path])
    run(["cmake", "--preset", preset] + cmake_configure_args)
    run(["cmake", "--build", "--preset", preset, "--parallel", str(os.cpu_count() or 1)])

    print(f"+ cp {build_output} {exe_name}")
    shutil.copy2(build_output, exe_name)

    copy_runtime_libs(is_windows, sdk_dir)


if __name__ == "__main__":
    main()
