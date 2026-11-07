#!/usr/bin/env python3
"""Zircon-owned host tool for transferring user data to CodeOS."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import tempfile
import zipfile


FORMAT = "move-on-1"
MAX_MANIFEST_BYTES = 16 * 1024 * 1024
MAX_ENTRIES = 100_000
MAX_TOTAL_BYTES = 16 * 1024 * 1024 * 1024
CHUNK_SIZE = 1024 * 1024
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)


class MoveOnError(Exception):
    """A user-correctable export/import error."""


def fail(message: str) -> None:
    raise MoveOnError(message)


def portable_relative(value: str) -> PurePosixPath:
    if not value or "\\" in value or "\0" in value or ":" in value:
        fail(f"invalid transfer path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in ("", ".", "..") for part in value.split("/")):
        fail(f"unsafe transfer path: {value!r}")
    if any(part.endswith((" ", ".")) for part in path.parts):
        fail(f"path is not portable across filesystems: {value!r}")
    if any(ord(char) < 32 or ord(char) == 127 for char in value):
        fail(f"control character in transfer path: {value!r}")
    return path


def zip_info(name: str, mode: int, is_dir: bool = False) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name + ("/" if is_dir else ""), ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = ((stat.S_IFDIR if is_dir else stat.S_IFREG) | mode) << 16
    return info


def add_file(archive: zipfile.ZipFile, path: Path, relative: str) -> dict:
    mode_before = path.lstat()
    if not stat.S_ISREG(mode_before.st_mode):
        fail(f"unsupported file type: {relative}")
    info = zip_info(f"payload/{relative}", stat.S_IMODE(mode_before.st_mode) & 0o777)
    digest = hashlib.sha256()
    size = 0
    try:
        fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
        with os.fdopen(fd, "rb") as source, archive.open(info, "w") as target:
            before = os.fstat(source.fileno())
            if not stat.S_ISREG(before.st_mode):
                fail(f"not a regular file: {relative}")
            while True:
                chunk = source.read(CHUNK_SIZE)
                if not chunk:
                    break
                size += len(chunk)
                if size > MAX_TOTAL_BYTES:
                    fail(f"file is too large: {relative}")
                digest.update(chunk)
                target.write(chunk)
            after = os.fstat(source.fileno())
    except OSError as exc:
        fail(f"cannot read {relative}: {exc}")
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns) != (
        after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns
    ):
        fail(f"file changed while exporting: {relative}")
    if size != after.st_size:
        fail(f"file changed while exporting: {relative}")
    return {
        "path": relative,
        "size": size,
        "sha256": digest.hexdigest(),
        "mode": stat.S_IMODE(after.st_mode) & 0o777,
    }


def export_tree(source_arg: str, output_arg: str) -> tuple[Path, int]:
    try:
        source = Path(source_arg).resolve(strict=True)
        output = Path(output_arg).resolve(strict=False)
    except OSError as exc:
        fail(f"cannot resolve export path: {exc}")
    if not source.is_dir():
        fail(f"source is not a directory: {source}")
    if output.suffix != ".moveon":
        fail("bundle filename must end in .moveon")
    if not output.parent.is_dir():
        fail(f"output directory does not exist: {output.parent}")
    if output.exists():
        fail(f"refusing to overwrite existing bundle: {output}")
    try:
        output.relative_to(source)
    except ValueError:
        pass
    else:
        fail("bundle must be outside the source directory")

    files: list[dict] = []
    directories: list[str] = []
    total_bytes = 0
    entries = 0
    created = False
    try:
        fd = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        created = True
        with os.fdopen(fd, "w+b") as output_file:
            with zipfile.ZipFile(output_file, "w", allowZip64=True) as archive:
                def walk_error(exc: OSError) -> None:
                    fail(f"cannot scan {exc.filename or source}: {exc}")

                for directory, dirnames, filenames in os.walk(
                    source, topdown=True, onerror=walk_error, followlinks=False
                ):
                    current = Path(directory)
                    dirnames.sort()
                    filenames.sort()
                    for dirname in dirnames:
                        path = current / dirname
                        relative = path.relative_to(source).as_posix()
                        portable_relative(relative)
                        if path.is_symlink() or not path.is_dir():
                            fail(f"symbolic links and special files are not allowed: {relative}")
                        entries += 1
                        if entries > MAX_ENTRIES:
                            fail(f"directory tree exceeds {MAX_ENTRIES} entries")
                        directories.append(relative)
                        archive.writestr(
                            zip_info(f"payload/{relative}", 0o755, is_dir=True), b""
                        )
                    for filename in filenames:
                        path = current / filename
                        relative = path.relative_to(source).as_posix()
                        portable_relative(relative)
                        files.append(add_file(archive, path, relative))
                        total_bytes += files[-1]["size"]
                        entries += 1
                        if entries > MAX_ENTRIES:
                            fail(f"directory tree exceeds {MAX_ENTRIES} entries")
                        if total_bytes > MAX_TOTAL_BYTES:
                            fail(f"directory tree exceeds {MAX_TOTAL_BYTES} bytes")
                manifest = {
                    "format": FORMAT,
                    "source_system": "Zircon",
                    "created_utc": datetime.now(timezone.utc).isoformat(),
                    "directories": directories,
                    "files": files,
                }
                manifest_bytes = (json.dumps(manifest, sort_keys=True, indent=2) + "\n").encode()
                if len(manifest_bytes) > MAX_MANIFEST_BYTES:
                    fail("transfer manifest is too large")
                archive.writestr(zip_info("manifest.json", 0o644), manifest_bytes)
        return output, len(files)
    except OSError as exc:
        if created:
            try:
                output.unlink()
            except FileNotFoundError:
                pass
        fail(f"cannot export bundle to {output}: {exc}")
    except BaseException:
        if created:
            try:
                output.unlink()
            except FileNotFoundError:
                pass
        raise


def load_bundle(archive: zipfile.ZipFile) -> tuple[dict, dict[str, zipfile.ZipInfo]]:
    entries = archive.infolist()
    if len(entries) > MAX_ENTRIES + 1:
        fail(f"bundle exceeds {MAX_ENTRIES} entries")
    by_name: dict[str, zipfile.ZipInfo] = {}
    for info in entries:
        if info.filename in by_name:
            fail(f"duplicate archive entry: {info.filename}")
        by_name[info.filename] = info

    manifest_info = by_name.get("manifest.json")
    if manifest_info is None or manifest_info.file_size > MAX_MANIFEST_BYTES:
        fail("bundle is missing a valid manifest.json")
    try:
        manifest = json.loads(archive.read(manifest_info).decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError, zipfile.BadZipFile) as exc:
        fail(f"invalid transfer manifest: {exc}")
    if not isinstance(manifest, dict) or manifest.get("format") != FORMAT:
        fail(f"bundle format must be {FORMAT}")
    if manifest.get("source_system") != "Zircon":
        fail("this importer only accepts bundles exported from Zircon")
    files = manifest.get("files")
    directories = manifest.get("directories")
    if not isinstance(files, list) or not isinstance(directories, list):
        fail("manifest files and directories must be lists")
    if len(files) + len(directories) > MAX_ENTRIES:
        fail(f"manifest exceeds {MAX_ENTRIES} entries")

    expected_names = {"manifest.json"}
    seen_paths: set[str] = set()
    files_by_path: dict[str, dict] = {}
    total_bytes = 0
    for relative in directories:
        if not isinstance(relative, str):
            fail("directory paths must be strings")
        portable_relative(relative)
        if relative in seen_paths:
            fail(f"duplicate transfer path: {relative}")
        seen_paths.add(relative)
        expected_names.add(f"payload/{relative}/")
    for item in files:
        if not isinstance(item, dict):
            fail("file records must be objects")
        relative = item.get("path")
        size = item.get("size")
        digest = item.get("sha256")
        mode = item.get("mode")
        if not isinstance(relative, str):
            fail("file path must be a string")
        portable_relative(relative)
        if relative in seen_paths:
            fail(f"duplicate transfer path: {relative}")
        seen_paths.add(relative)
        files_by_path[relative] = item
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            fail(f"invalid file size for {relative}")
        if not isinstance(digest, str) or len(digest) != 64 or any(
            char not in "0123456789abcdef" for char in digest
        ):
            fail(f"invalid SHA-256 for {relative}")
        if not isinstance(mode, int) or isinstance(mode, bool) or mode < 0 or mode > 0o777:
            fail(f"invalid file mode for {relative}")
        total_bytes += size
        if total_bytes > MAX_TOTAL_BYTES:
            fail(f"bundle exceeds {MAX_TOTAL_BYTES} uncompressed bytes")
        expected_names.add(f"payload/{relative}")
    if set(by_name) != expected_names:
        fail("archive entries do not match the manifest")
    for name in expected_names - {"manifest.json"}:
        info = by_name[name]
        if info.file_size < 0 or info.file_size > MAX_TOTAL_BYTES:
            fail(f"invalid archive size: {name}")
        if name.endswith("/"):
            file_type = (info.external_attr >> 16) & 0o170000
            if not info.is_dir() or info.file_size != 0 or file_type not in (0, stat.S_IFDIR):
                fail(f"expected a directory entry: {name}")
        else:
            if info.is_dir():
                fail(f"expected a regular file: {name}")
            file_record = files_by_path[name.removeprefix("payload/")]
            if info.file_size != file_record["size"]:
                fail(f"manifest size mismatch: {name}")
            file_type = (info.external_attr >> 16) & 0o170000
            if file_type not in (0, stat.S_IFREG):
                fail(f"unsupported file type: {name}")
    return manifest, by_name


def import_tree(bundle_arg: str, destination_arg: str) -> tuple[Path, int]:
    try:
        bundle = Path(bundle_arg).resolve(strict=True)
        destination = Path(destination_arg).absolute()
        parent = destination.parent.resolve(strict=True)
    except OSError as exc:
        fail(f"cannot resolve import path: {exc}")
    destination = parent / destination.name
    if not bundle.is_file():
        fail(f"bundle is not a regular file: {bundle}")
    if destination.exists():
        fail(f"refusing to overwrite existing destination: {destination}")
    try:
        with zipfile.ZipFile(bundle, "r") as archive:
            manifest, by_name = load_bundle(archive)
            stage = Path(tempfile.mkdtemp(prefix=".move-on-", dir=parent))
            try:
                for relative in manifest["directories"]:
                    target = stage.joinpath(*portable_relative(relative).parts)
                    target.mkdir(parents=True, exist_ok=True)
                for item in manifest["files"]:
                    relative = item["path"]
                    target = stage.joinpath(*portable_relative(relative).parts)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    digest = hashlib.sha256()
                    size = 0
                    with archive.open(by_name[f"payload/{relative}"], "r") as source, target.open("xb") as output:
                        while True:
                            chunk = source.read(CHUNK_SIZE)
                            if not chunk:
                                break
                            size += len(chunk)
                            digest.update(chunk)
                            output.write(chunk)
                    if size != item["size"] or digest.hexdigest() != item["sha256"]:
                        fail(f"integrity check failed: {relative}")
                    target.chmod(item["mode"])
                for relative in sorted(manifest["directories"], key=lambda path: path.count("/"), reverse=True):
                    stage.joinpath(*portable_relative(relative).parts).chmod(0o755)
                if destination.exists():
                    fail(f"refusing to overwrite existing destination: {destination}")
                os.rename(stage, destination)
            except BaseException:
                shutil.rmtree(stage, ignore_errors=True)
                raise
    except (OSError, EOFError, NotImplementedError, zipfile.BadZipFile, RuntimeError) as exc:
        fail(f"cannot import bundle: {exc}")
    return destination, len(manifest["files"])


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Move personal data from Zircon to CodeOS using a portable .moveon bundle."
    )
    commands = parser.add_subparsers(dest="command", required=True)
    export_parser = commands.add_parser("export", help="export a Zircon data directory")
    export_parser.add_argument("source", help="source directory from Zircon")
    export_parser.add_argument("--output", required=True, help="new .moveon bundle path")
    import_parser = commands.add_parser("import", help="import a Zircon bundle into CodeOS")
    import_parser.add_argument("bundle", help="input .moveon bundle")
    import_parser.add_argument("--destination", required=True, help="new destination directory")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        if args.command == "export":
            path, count = export_tree(args.source, args.output)
            print(f"Move On: exported {count} files from Zircon to {path}")
        else:
            path, count = import_tree(args.bundle, args.destination)
            print(f"Move On: imported {count} files into CodeOS at {path}")
    except MoveOnError as exc:
        print(f"move-on: error: {exc}", file=__import__("sys").stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
