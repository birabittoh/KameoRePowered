#!/usr/bin/env python3
import os
import sys
import glob
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


def find_clangxx():
    # Versioned binaries (clang++-20, clang++-22, …) only exist on Linux.
    if platform.system() != "Windows":
        for version in range(30, 17, -1):
            if shutil.which(f"clang++-{version}"):
                return f"clang++-{version}"
    if shutil.which("clang++"):
        return "clang++"
    print("error: no clang++ compiler found in PATH", file=sys.stderr)
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
    src_dir = os.path.join(sdk_dir, "bin" if is_windows else "lib")
    suffix = ".dll" if is_windows else ".so"

    if not os.path.isdir(src_dir):
        return
    for name in os.listdir(src_dir):
        if name.endswith(suffix):
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
    p.add_argument(
        "--tu",
        nargs="+",
        metavar="PACKAGE",
        help="Build with a title update. Pass the TU package(s) (LIVE/CON/PIRS) or a "
        "directory containing them; the variant matching your base XEX is selected by "
        "digest, staged as a sibling .xexp, and baked in by codegen.",
    )
    return p.parse_args()


def main():
    args = parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.normpath(os.path.join(script_dir, ".."))
    os.chdir(root)

    is_windows = platform.system() == "Windows"

    manifests = glob.glob("*_manifest.toml")
    if len(manifests) != 1:
        print(
            f"error: expected exactly one *_manifest.toml in the repo root, "
            f"found: {manifests if manifests else 'none'}",
            file=sys.stderr,
        )
        sys.exit(1)
    manifest_path = manifests[0]
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

    cxx_compiler = find_clangxx()

    cmake_configure_args = [
        f"-DCMAKE_PREFIX_PATH={sdk_dir}",
        f"-DCMAKE_CXX_COMPILER={cxx_compiler}",
        # Always set explicitly so switching between TU and vanilla builds doesn't
        # inherit a stale value from the CMake cache.
        f"-DKAMEO_TU={'ON' if args.tu else 'OFF'}",
    ]
    if shutil.which("sccache"):
        cmake_configure_args += [
            "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache",
        ]

    tu_version = None
    sibling_patch = xex_path + "p"
    codegen_manifest = manifest_path
    if args.tu:
        sys.path.insert(0, script_dir)
        import build
        _, tu_version = build.stage_title_update(args.tu, xex_path)
        overrides = "kameorepowered_tu_overrides.toml"
        base_config = manifest["entrypoint"]["includes"][0]
        if os.path.exists(overrides):
            codegen_manifest, _ = build.derive_tu_manifest(manifest_path, base_config, overrides)
        else:
            print(f"warning: {overrides} not found; using vanilla codegen hints", file=sys.stderr)
    elif os.path.exists(sibling_patch):
        print(
            f"warning: '{sibling_patch}' is present and will be baked into this build by "
            f"codegen. Remove it or pass --tu for an explicit title-update build.",
            file=sys.stderr,
        )

    # codegen status ignored (temporary workaround)
    print(f"+ {rexglue} codegen {codegen_manifest}")
    subprocess.run([rexglue, "codegen", codegen_manifest])
    run(["cmake", "--preset", preset] + cmake_configure_args)
    run(["cmake", "--build", "--preset", preset, "--parallel", str(os.cpu_count() or 1)])

    print(f"+ cp {build_output} {exe_name}")
    shutil.copy2(build_output, exe_name)

    copy_runtime_libs(is_windows, sdk_dir)

    if tu_version:
        print(
            f"\nBuilt with title update v{tu_version}. The matching patch is staged at "
            f"'{sibling_patch}'\nand is required at runtime — the loader re-applies it to the "
            f"base image on launch.\nPlayers supply their own dump + TU; "
            f"scripts/extract_tu.py --base <default.xex> selects the right one."
        )


if __name__ == "__main__":
    main()
