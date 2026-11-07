#!/usr/bin/env python3
"""Build and consume signed Zircon ZircApp repositories."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import ssl
import stat
import subprocess
import tarfile
import tempfile
from urllib.error import URLError
from urllib.parse import urljoin, urlparse
from urllib.request import urlopen


INDEX_FORMAT = "zircon-app-index-1"
APP_FORMAT = "zircon-app-1"
MAX_INDEX_BYTES = 1024 * 1024
MAX_ARCHIVE_BYTES = 16 * 1024 * 1024
MAX_FILE_BYTES = 8 * 1024 * 1024
MAX_TOTAL_BYTES = 32 * 1024 * 1024
MAX_FILES = 256
PERMISSIONS = {
    "audio",
    "camera",
    "files.read",
    "files.write",
    "network",
    "notifications",
    "phone",
    "sensors",
    "system",
    "time",
}
NAME_RE = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")
VERSION_RE = re.compile(r"^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[0-9A-Za-z.-]+)?$")


class RepoError(Exception):
    """A validation, repository, or install error suitable for CLI display."""


def fail(message: str) -> "NoReturn":
    raise RepoError(message)


def validate_manifest(value: object) -> dict:
    if not isinstance(value, dict):
        fail("manifest must be a JSON object")
    required = ("format", "name", "version", "description", "category",
                "runtime", "abi", "install_path", "entrypoint", "permissions",
                "dependencies")
    for field in required:
        if field not in value:
            fail(f"manifest is missing '{field}'")
    if value["format"] != APP_FORMAT:
        fail(f"manifest format must be {APP_FORMAT}")
    if not isinstance(value["name"], str) or not NAME_RE.fullmatch(value["name"]):
        fail("name must be a lowercase package identifier")
    if not isinstance(value["version"], str) or not VERSION_RE.fullmatch(value["version"]):
        fail("version must use MAJOR.MINOR.PATCH")
    if not isinstance(value["description"], str) or not value["description"].strip() or len(value["description"]) > 200:
        fail("description must contain 1 to 200 characters")
    if not isinstance(value["category"], str) or not value["category"].strip() or len(value["category"]) > 32:
        fail("category must contain 1 to 32 characters")
    if value["runtime"] != "zircapp":
        fail("runtime must be 'zircapp'")
    if value["abi"] != 1:
        fail("unsupported ZircApp ABI (expected 1)")
    expected_install_path = f"/apps/{value['name']}"
    if value["install_path"] != expected_install_path:
        fail(f"install_path must be '{expected_install_path}'")
    entrypoint = value["entrypoint"]
    path = PurePosixPath(entrypoint) if isinstance(entrypoint, str) else None
    if (path is None or path.is_absolute() or not path.parts or
            "\\" in entrypoint or "\x00" in entrypoint or
            any(part in ("", ".", "..") for part in path.parts)):
        fail("entrypoint must be a safe relative path")
    permissions = value["permissions"]
    if (not isinstance(permissions, list) or
            any(not isinstance(item, str) or item not in PERMISSIONS for item in permissions) or
            len(set(permissions)) != len(permissions)):
        fail("permissions must be a unique list of supported capabilities")
    dependencies = value["dependencies"]
    if (not isinstance(dependencies, list) or
            any(not isinstance(item, str) or not NAME_RE.fullmatch(item) for item in dependencies) or
            len(set(dependencies)) != len(dependencies) or value["name"] in dependencies):
        fail("dependencies must be unique package names and cannot include the app itself")
    return value


def read_manifest_bytes(data: bytes) -> dict:
    try:
        value = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"invalid manifest JSON: {exc}")
    return validate_manifest(value)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def archive_manifest(archive_bytes: bytes) -> tuple[dict, list[tarfile.TarInfo]]:
    if len(archive_bytes) > MAX_ARCHIVE_BYTES:
        fail("package archive exceeds the 16 MiB limit")
    try:
        archive = tarfile.open(fileobj=io.BytesIO(archive_bytes), mode="r:gz")
    except (tarfile.TarError, OSError) as exc:
        fail(f"invalid .xora archive: {exc}")
    with archive:
        members = archive.getmembers()
        if len(members) > MAX_FILES + 16:
            fail("package archive has too many entries")
        manifest_members = [m for m in members if m.name == "manifest.json"]
        if len(manifest_members) != 1 or not manifest_members[0].isfile():
            fail("archive must contain one regular root manifest.json")
        manifest_file = archive.extractfile(manifest_members[0])
        if manifest_file is None:
            fail("cannot read archive manifest")
        manifest_data = manifest_file.read(64 * 1024 + 1)
        if len(manifest_data) > 64 * 1024:
            fail("manifest exceeds the 64 KiB limit")
        manifest = read_manifest_bytes(manifest_data)
        expected_root = PurePosixPath("payload") / "apps" / manifest["name"]
        file_count = 0
        total_bytes = 0
        seen: set[str] = set()
        for member in members:
            path = PurePosixPath(member.name)
            canonical_name = path.as_posix() + ("/" if member.isdir() else "")
            if member.name != canonical_name:
                fail(f"non-canonical archive path: {member.name}")
            if member.name in seen:
                fail(f"duplicate archive path: {member.name}")
            seen.add(member.name)
            if member.name == "manifest.json":
                continue
            if path.is_absolute() or any(part in ("", ".", "..") for part in path.parts):
                fail(f"unsafe archive path: {member.name}")
            if path.parts[:3] != expected_root.parts:
                fail(f"payload must stay below {expected_root}: {member.name}")
            if member.issym() or member.islnk() or member.isdev() or member.isfifo():
                fail(f"unsupported archive entry: {member.name}")
            if not (member.isfile() or member.isdir()):
                fail(f"unsupported archive entry: {member.name}")
            if member.isfile():
                file_count += 1
                if member.size > MAX_FILE_BYTES:
                    fail(f"file exceeds the 8 MiB limit: {member.name}")
                total_bytes += member.size
                if total_bytes > MAX_TOTAL_BYTES:
                    fail("package payload exceeds the 32 MiB total limit")
        if file_count == 0 or file_count > MAX_FILES:
            fail("package must contain between 1 and 256 regular payload files")
        wanted = expected_root / PurePosixPath(manifest["entrypoint"])
        if str(wanted) not in seen:
            fail(f"entrypoint is absent from archive: {wanted}")
        if not any(m.name == str(wanted) and m.isfile() and (m.mode & 0o111) for m in members):
            fail("entrypoint must be a regular executable file")
        return manifest, members


def canonical_json(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def load_index(data: bytes) -> dict:
    if len(data) > MAX_INDEX_BYTES:
        fail("repository index exceeds the 1 MiB limit")
    try:
        index = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"invalid repository index: {exc}")
    if not isinstance(index, dict) or index.get("format") != INDEX_FORMAT:
        fail(f"index format must be {INDEX_FORMAT}")
    apps = index.get("apps")
    if not isinstance(apps, list) or len(apps) > 4096:
        fail("index apps must be a list of at most 4096 entries")
    seen: set[tuple[str, str]] = set()
    for item in apps:
        if not isinstance(item, dict):
            fail("each index entry must be an object")
        for field in ("name", "version", "archive", "sha256", "size"):
            if field not in item:
                fail(f"index entry is missing '{field}'")
        if not isinstance(item["name"], str) or not isinstance(item["version"], str):
            fail("index entry name and version must be strings")
        key = (item["name"], item["version"])
        if key in seen:
            fail(f"duplicate package in index: {key[0]} {key[1]}")
        seen.add(key)
        if (not isinstance(item["archive"], str) or
                not re.fullmatch(r"[A-Za-z0-9._-]+\.xora", item["archive"]) or
                item["archive"] in (".xora", "..xora")):
            fail("archive must be a filename, not a path or URL")
        if not isinstance(item["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
            fail("index entry has an invalid SHA-256 digest")
        if not isinstance(item["size"], int) or not 0 < item["size"] <= MAX_ARCHIVE_BYTES:
            fail("index entry has an invalid archive size")
        validate_manifest({
            "format": APP_FORMAT, "name": item["name"], "version": item["version"],
            "description": item.get("description"), "category": item.get("category"),
            "runtime": item.get("runtime"), "abi": item.get("abi"),
            "install_path": item.get("install_path"), "entrypoint": item.get("entrypoint"),
            "permissions": item.get("permissions"), "dependencies": item.get("dependencies"),
        })
    return index


def verify_signature(data: bytes, signature: bytes, public_key: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="zircon-index-") as directory:
        index_path = Path(directory) / "index.json"
        signature_path = Path(directory) / "index.sig"
        index_path.write_bytes(data)
        signature_path.write_bytes(signature)
        result = subprocess.run(
            ["openssl", "pkeyutl", "-verify", "-pubin", "-inkey", str(public_key),
             "-rawin", "-in", str(index_path), "-sigfile", str(signature_path)],
            capture_output=True, text=True, check=False,
        )
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip() or "signature mismatch"
        fail(f"repository signature verification failed: {detail}")


def sign_index(index_path: Path, private_key: Path, signature_path: Path) -> None:
    result = subprocess.run(
        ["openssl", "pkeyutl", "-sign", "-inkey", str(private_key), "-rawin",
         "-in", str(index_path), "-out", str(signature_path)],
        capture_output=True, text=True, check=False,
    )
    if result.returncode:
        fail(f"could not sign repository index: {result.stderr.strip()}")


def index_repository(repo_dir: Path) -> dict:
    if not repo_dir.is_dir():
        fail(f"repository directory not found: {repo_dir}")
    apps = []
    for path in sorted(repo_dir.glob("*.xora")):
        if path.is_symlink() or not path.is_file():
            fail(f"repository packages must be regular files: {path.name}")
        if path.stat().st_size > MAX_ARCHIVE_BYTES:
            fail(f"package archive exceeds the 16 MiB limit: {path.name}")
        data = path.read_bytes()
        manifest, _ = archive_manifest(data)
        apps.append({
            "name": manifest["name"],
            "version": manifest["version"],
            "description": manifest["description"],
            "category": manifest["category"],
            "runtime": manifest["runtime"],
            "abi": manifest["abi"],
            "install_path": manifest["install_path"],
            "entrypoint": manifest["entrypoint"],
            "permissions": manifest["permissions"],
            "dependencies": manifest["dependencies"],
            "archive": path.name,
            "sha256": sha256(data),
            "size": len(data),
        })
    if not apps:
        fail(f"no .xora app packages found in {repo_dir}")
    return {"format": INDEX_FORMAT, "apps": apps}


def pack_source(source_dir: Path, output_path: Path) -> dict:
    source_dir = source_dir.resolve()
    output_path = output_path.resolve()
    if not source_dir.is_dir():
        fail(f"app source directory not found: {source_dir}")
    if output_path.suffix != ".xora":
        fail("package output must use the .xora extension")
    if output_path == source_dir or source_dir in output_path.parents:
        fail("package output must be outside the source directory")
    manifest_path = source_dir / "manifest.json"
    if manifest_path.is_symlink() or not manifest_path.is_file():
        fail("app manifest must be a regular file")
    try:
        manifest = validate_manifest(json.loads(manifest_path.read_text(encoding="utf-8")))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"cannot read app manifest: {exc}")
    payload_root = source_dir / "payload"
    if not payload_root.is_dir() or payload_root.is_symlink():
        fail("app source must contain a regular payload/ directory")

    buffer = io.BytesIO()
    file_count = 0
    with tarfile.open(fileobj=buffer, mode="w:gz", compresslevel=9) as archive:
        manifest_data = canonical_json(manifest)
        manifest_info = tarfile.TarInfo("manifest.json")
        manifest_info.mode = 0o644
        manifest_info.size = len(manifest_data)
        archive.addfile(manifest_info, io.BytesIO(manifest_data))
        for path in sorted(payload_root.rglob("*")):
            relative = path.relative_to(payload_root).as_posix()
            payload_path = PurePosixPath(relative)
            if (payload_path.is_absolute() or "\\" in relative or
                    any(part in ("", ".", "..") for part in payload_path.parts)):
                fail(f"unsafe source payload path: {relative}")
            full_name = PurePosixPath("payload") / payload_path
            expected_root = PurePosixPath("payload") / "apps" / manifest["name"]
            if path.is_symlink():
                fail(f"symbolic links are not allowed: {relative}")
            if path.is_dir():
                parts = full_name.parts
                is_parent = expected_root.parts[:len(parts)] == parts
                is_child = parts[:len(expected_root.parts)] == expected_root.parts
                if not (is_parent or is_child):
                    fail(f"payload must stay below {expected_root}: {relative}")
                continue
            if full_name.parts[:len(expected_root.parts)] != expected_root.parts:
                fail(f"payload must stay below {expected_root}: {relative}")
            if not path.is_file():
                fail(f"unsupported source file type: {relative}")
            size = path.stat().st_size
            if size > MAX_FILE_BYTES:
                fail(f"file exceeds the 8 MiB limit: {relative}")
            file_count += 1
            if file_count > MAX_FILES:
                fail("package exceeds the 256 file limit")
            info = tarfile.TarInfo(full_name.as_posix())
            info.mode = stat.S_IMODE(path.stat().st_mode) & 0o755 or 0o644
            info.size = size
            with path.open("rb") as handle:
                archive.addfile(info, handle)
    package_bytes = buffer.getvalue()
    archive_manifest(package_bytes)
    if len(package_bytes) > MAX_ARCHIVE_BYTES:
        fail("generated package archive exceeds the 16 MiB limit")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(package_bytes)
    return manifest


def install_package(index: dict, archive_bytes: bytes, root: Path,
                    package_name: str, package_version: str | None = None) -> None:
    matches = [item for item in index["apps"] if item["name"] == package_name and
               (package_version is None or item["version"] == package_version)]
    if len(matches) != 1:
        fail(f"expected exactly one matching app entry for {package_name}")
    item = matches[0]
    if len(archive_bytes) != item["size"] or sha256(archive_bytes) != item["sha256"]:
        fail("package archive size or SHA-256 does not match the signed index")
    manifest, _ = archive_manifest(archive_bytes)
    if manifest["name"] != item["name"] or manifest["version"] != item["version"]:
        fail("package manifest does not match the signed index entry")
    root = root.resolve()
    if not root.is_dir():
        fail(f"installation root does not exist: {root}")
    state_path, state_dir, installed = load_installed_state(root, create=True)
    old_state = state_path.read_bytes() if state_path.exists() else None
    installed_names = {record["name"] for record in installed["apps"]}
    missing_dependencies = [name for name in manifest["dependencies"]
                           if name not in installed_names]
    if missing_dependencies:
        fail("missing package dependencies: " + ", ".join(missing_dependencies))

    apps_dir = root / "apps"
    if apps_dir.is_symlink():
        fail("installation apps directory must not be a symbolic link")
    apps_dir.mkdir(exist_ok=True)
    target = apps_dir / manifest["name"]
    stage = Path(tempfile.mkdtemp(prefix=f".{manifest['name']}.stage-", dir=apps_dir))
    backup = apps_dir / f".{manifest['name']}.backup"
    state_temp = None
    state_committed = False
    try:
        if target.is_symlink() or backup.is_symlink():
            fail("existing app and backup paths must not be symbolic links")
        if backup.exists():
            fail(f"stale app backup exists: {backup}")
        with tarfile.open(fileobj=io.BytesIO(archive_bytes), mode="r:gz") as archive:
            for member in archive.getmembers():
                if not member.isfile() or member.name == "manifest.json":
                    continue
                source = archive.extractfile(member)
                if source is None:
                    fail(f"cannot read package file: {member.name}")
                relative = PurePosixPath(member.name).relative_to(
                    PurePosixPath("payload") / "apps" / manifest["name"]
                )
                destination = stage.joinpath(*relative.parts)
                destination.parent.mkdir(parents=True, exist_ok=True)
                with source, destination.open("xb") as output:
                    shutil.copyfileobj(source, output, length=64 * 1024)
                destination.chmod(member.mode & 0o755 or 0o644)
        new_record = {
            "name": manifest["name"],
            "version": manifest["version"],
            "entrypoint": manifest["entrypoint"],
            "permissions": manifest["permissions"],
            "dependencies": manifest["dependencies"],
            "sha256": item["sha256"],
        }
        installed["apps"] = [record for record in installed["apps"]
                             if record["name"] != manifest["name"]]
        installed["apps"].append(new_record)
        installed["apps"].sort(key=lambda record: record["name"])
        state_temp = write_state_temp(state_dir, installed)
        if backup.exists():
            remove_path(backup)
        if target.exists():
            target.rename(backup)
        try:
            stage.rename(target)
            os.replace(state_temp, state_path)
            state_temp = None
            state_committed = True
            sync_directory(state_dir)
            sync_directory(apps_dir)
        except Exception as exc:
            if target.exists():
                remove_path(target)
            if backup.exists() and not target.exists():
                backup.rename(target)
            if state_committed:
                restore_state(state_path, state_dir, old_state)
            raise exc
        if backup.exists():
            remove_path(backup)
    except Exception:
        if stage.exists():
            remove_path(stage)
        if state_temp and state_temp.exists():
            state_temp.unlink()
        raise


def remove_path(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)


def sync_directory(path: Path) -> None:
    descriptor = os.open(path, os.O_RDONLY)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def load_installed_state(root: Path, create: bool = False) -> tuple[Path, Path, dict]:
    state_dir = root / "var" / "lib" / "zircon"
    for path in (root / "var", root / "var" / "lib", state_dir):
        if path.is_symlink():
            fail(f"state directory must not be a symbolic link: {path}")
        if create:
            path.mkdir(exist_ok=True)
    state_path = state_dir / "apps.json"
    if state_path.is_symlink():
        fail(f"installed-app database must not be a symbolic link: {state_path}")
    if not state_path.exists():
        return state_path, state_dir, {"format": "zircon-installed-apps-1", "apps": []}
    try:
        raw_state = state_path.read_bytes()
        if len(raw_state) > MAX_INDEX_BYTES:
            fail("installed-app database exceeds the 1 MiB limit")
        state = json.loads(raw_state.decode("utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"invalid installed-app database: {exc}")
    if (not isinstance(state, dict) or state.get("format") != "zircon-installed-apps-1" or
            not isinstance(state.get("apps"), list)):
        fail("installed-app database has an unsupported format")
    names = set()
    for record in state["apps"]:
        if (not isinstance(record, dict) or not isinstance(record.get("name"), str) or
                not NAME_RE.fullmatch(record["name"]) or record["name"] in names):
            fail("installed-app database contains an invalid or duplicate app entry")
        names.add(record["name"])
    return state_path, state_dir, state


def write_state_temp(state_dir: Path, state: dict) -> Path:
    descriptor, name = tempfile.mkstemp(prefix=".apps-", suffix=".tmp", dir=state_dir)
    path = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(canonical_json(state))
            handle.flush()
            os.fsync(handle.fileno())
    except Exception:
        path.unlink(missing_ok=True)
        raise
    return path


def restore_state(state_path: Path, state_dir: Path, old_state: bytes | None) -> None:
    if old_state is None:
        state_path.unlink(missing_ok=True)
        sync_directory(state_dir)
        return
    descriptor, name = tempfile.mkstemp(prefix=".apps-rollback-", suffix=".tmp", dir=state_dir)
    temp_path = Path(name)
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(old_state)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_path, state_path)
        sync_directory(state_dir)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def remove_package(root: Path, package_name: str) -> None:
    if not NAME_RE.fullmatch(package_name):
        fail("invalid package name")
    root = root.resolve()
    if not root.is_dir():
        fail(f"installation root does not exist: {root}")
    state_path, state_dir, state = load_installed_state(root)
    old_state = state_path.read_bytes()
    record = next((item for item in state["apps"] if item["name"] == package_name), None)
    if record is None:
        fail(f"app is not installed: {package_name}")
    dependents = [item["name"] for item in state["apps"]
                  if package_name in item.get("dependencies", [])]
    if dependents:
        fail("app is required by installed packages: " + ", ".join(sorted(dependents)))

    apps_dir = root / "apps"
    if apps_dir.is_symlink():
        fail("installation apps directory must not be a symbolic link")
    target = apps_dir / package_name
    backup = apps_dir / f".{package_name}.backup"
    if target.is_symlink() or backup.is_symlink() or backup.exists():
        fail("app or backup path is unsafe or stale")
    state["apps"] = [item for item in state["apps"] if item["name"] != package_name]
    state_temp = write_state_temp(state_dir, state)
    state_committed = False
    try:
        if target.exists():
            target.rename(backup)
        try:
            os.replace(state_temp, state_path)
            state_committed = True
            sync_directory(state_dir)
            sync_directory(apps_dir)
        except Exception as exc:
            if backup.exists() and not target.exists():
                backup.rename(target)
            if state_committed:
                restore_state(state_path, state_dir, old_state)
            raise exc
        if backup.exists():
            remove_path(backup)
    except Exception:
        if state_temp.exists():
            state_temp.unlink()
        raise


def fetch_https(url: str, limit: int) -> bytes:
    parsed = urlparse(url)
    if parsed.scheme != "https" or not parsed.hostname:
        fail("repository URLs must use HTTPS")
    try:
        with urlopen(url, timeout=30, context=ssl.create_default_context()) as response:
            final_url = urlparse(response.geturl())
            if (final_url.scheme != "https" or final_url.hostname != parsed.hostname or
                    final_url.port != parsed.port):
                fail("repository download redirected away from its HTTPS origin")
            data = response.read(limit + 1)
    except (URLError, TimeoutError, OSError) as exc:
        fail(f"repository download failed: {exc}")
    if len(data) > limit:
        fail("download exceeds the configured size limit")
    return data


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)

    validate = commands.add_parser("validate", help="validate an app manifest or .xora archive")
    validate.add_argument("path")

    pack = commands.add_parser("pack", help="pack and validate a built ZircApp payload")
    pack.add_argument("source")
    pack.add_argument("-o", "--output", required=True)

    index = commands.add_parser("index", help="create a catalog from a directory of .xora packages")
    index.add_argument("repository")
    index.add_argument("-o", "--output", required=True)

    sign = commands.add_parser("sign", help="sign an index using an Ed25519 PEM private key")
    sign.add_argument("index")
    sign.add_argument("--key", required=True)
    sign.add_argument("--signature", required=True)

    verify = commands.add_parser("verify", help="verify a signed repository index")
    verify.add_argument("index")
    verify.add_argument("--signature", required=True)
    verify.add_argument("--public-key", required=True)

    install = commands.add_parser("install", help="install one indexed app into a target root")
    install.add_argument("index")
    install.add_argument("--signature", required=True)
    install.add_argument("--public-key", required=True)
    install.add_argument("--archive", required=True)
    install.add_argument("--name", required=True)
    install.add_argument("--version")
    install.add_argument("--root", required=True)

    remove = commands.add_parser("remove", help="remove a previously installed app bundle")
    remove.add_argument("--name", required=True)
    remove.add_argument("--root", required=True)

    sync = commands.add_parser("sync", help="download and install one app from an HTTPS repository")
    sync.add_argument("repository", help="base HTTPS URL containing index.json, index.sig, and packages")
    sync.add_argument("--public-key", required=True)
    sync.add_argument("--name", required=True)
    sync.add_argument("--version")
    sync.add_argument("--root", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "validate":
            path = Path(args.path)
            if path.suffix == ".xora":
                manifest, members = archive_manifest(path.read_bytes())
                print(f"{manifest['name']} {manifest['version']}: {len(members)} archive entries, valid")
            else:
                manifest = validate_manifest(json.loads(path.read_text(encoding="utf-8")))
                print(f"{manifest['name']} {manifest['version']}: valid ZircApp manifest")
        elif args.command == "pack":
            manifest = pack_source(Path(args.source), Path(args.output))
            print(f"packed {manifest['name']} {manifest['version']} -> {args.output}")
        elif args.command == "index":
            data = canonical_json(index_repository(Path(args.repository)))
            if len(data) > MAX_INDEX_BYTES:
                fail("generated index exceeds the 1 MiB limit")
            Path(args.output).write_bytes(data)
            print(f"wrote {args.output}")
        elif args.command == "sign":
            sign_index(Path(args.index), Path(args.key), Path(args.signature))
            print(f"signed {args.index} -> {args.signature}")
        elif args.command == "verify":
            data = Path(args.index).read_bytes()
            index = load_index(data)
            verify_signature(data, Path(args.signature).read_bytes(), Path(args.public_key))
            print(f"verified {len(index['apps'])} app entries")
        elif args.command == "install":
            data = Path(args.index).read_bytes()
            index = load_index(data)
            verify_signature(data, Path(args.signature).read_bytes(), Path(args.public_key))
            install_package(index, Path(args.archive).read_bytes(), Path(args.root),
                            args.name, args.version)
            print(f"installed {args.name} into {Path(args.root).resolve() / 'apps' / args.name}")
        elif args.command == "remove":
            remove_package(Path(args.root), args.name)
            print(f"removed {args.name}")
        elif args.command == "sync":
            base = args.repository.rstrip("/") + "/"
            index_url = urljoin(base, "index.json")
            signature_url = urljoin(base, "index.sig")
            index_data = fetch_https(index_url, MAX_INDEX_BYTES)
            index = load_index(index_data)
            verify_signature(index_data, fetch_https(signature_url, 4096), Path(args.public_key))
            matches = [entry for entry in index["apps"] if entry["name"] == args.name and
                       (args.version is None or entry["version"] == args.version)]
            if len(matches) != 1:
                fail(f"expected exactly one matching repository entry for {args.name}")
            entry = matches[0]
            archive_data = fetch_https(urljoin(base, entry["archive"]), MAX_ARCHIVE_BYTES)
            install_package(index, archive_data, Path(args.root), args.name, args.version)
            print(f"installed {args.name} {entry['version']}")
        return 0
    except (RepoError, OSError, json.JSONDecodeError, tarfile.TarError, subprocess.SubprocessError) as exc:
        print(f"zircapp-repo: error: {exc}", file=os.sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
