import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import zircapp_repo as repo


def make_archive(name="demo", version="1.2.3", payload_path=None, dependencies=None):
    manifest = {
        "format": repo.APP_FORMAT,
        "name": name,
        "version": version,
        "description": "A test app",
        "category": "Tools",
        "runtime": "zircapp",
        "abi": 1,
        "install_path": f"/apps/{name}",
        "entrypoint": "bin/demo",
        "permissions": ["network"],
        "dependencies": dependencies or [],
    }
    data = io.BytesIO()
    with tarfile.open(fileobj=data, mode="w:gz") as archive:
        manifest_bytes = json.dumps(manifest).encode()
        entry = tarfile.TarInfo("manifest.json")
        entry.size = len(manifest_bytes)
        archive.addfile(entry, io.BytesIO(manifest_bytes))
        payload = b"demo executable"
        binary = tarfile.TarInfo(payload_path or f"payload/apps/{name}/bin/demo")
        binary.mode = 0o755
        binary.size = len(payload)
        archive.addfile(binary, io.BytesIO(payload))
    return data.getvalue()


def index_entry(archive_data, archive_name):
    manifest, _ = repo.archive_manifest(archive_data)
    return {
        **{field: manifest[field] for field in (
            "name", "version", "description", "category", "runtime",
            "abi", "install_path", "entrypoint", "permissions", "dependencies")},
        "archive": archive_name,
        "sha256": repo.sha256(archive_data),
        "size": len(archive_data),
    }


class ZirconAppRepoTests(unittest.TestCase):
    def test_manifest_validation_accepts_supported_app(self):
        manifest, _ = repo.archive_manifest(make_archive())
        self.assertEqual(manifest["name"], "demo")

    def test_manifest_rejects_unsafe_install_path_and_capabilities(self):
        manifest = {
            "format": repo.APP_FORMAT,
            "name": "demo",
            "version": "1.0.0",
            "description": "A test app",
            "category": "Tools",
            "runtime": "zircapp",
            "abi": 1,
            "install_path": "/apps/other",
            "entrypoint": "bin/demo",
            "permissions": ["root"],
            "dependencies": [],
        }
        with self.assertRaises(repo.RepoError):
            repo.validate_manifest(manifest)
        manifest["install_path"] = "/apps/demo"
        with self.assertRaises(repo.RepoError):
            repo.validate_manifest(manifest)

    def test_archive_rejects_traversal(self):
        with self.assertRaises(repo.RepoError):
            repo.archive_manifest(make_archive(payload_path="payload/apps/demo/../escape"))

    def test_pack_source_builds_valid_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            payload = source / "payload/apps/demo/bin/demo"
            payload.parent.mkdir(parents=True)
            payload.write_bytes(b"packaged app")
            payload.chmod(0o755)
            manifest = {
                "format": repo.APP_FORMAT,
                "name": "demo",
                "version": "1.2.3",
                "description": "A test app",
                "category": "Tools",
                "runtime": "zircapp",
                "abi": 1,
                "install_path": "/apps/demo",
                "entrypoint": "bin/demo",
                "permissions": [],
                "dependencies": [],
            }
            (source / "manifest.json").write_text(json.dumps(manifest))
            output = root / "repository/demo-1.2.3.xora"
            repo.pack_source(source, output)
            packed, _ = repo.archive_manifest(output.read_bytes())
            self.assertEqual(packed["name"], "demo")

    def test_repository_index_is_derived_from_validated_archives(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            payload = source / "payload/apps/demo/bin/demo"
            payload.parent.mkdir(parents=True)
            payload.write_bytes(b"app")
            payload.chmod(0o755)
            manifest = {
                "format": repo.APP_FORMAT,
                "name": "demo",
                "version": "1.2.3",
                "description": "A test app",
                "category": "Tools",
                "runtime": "zircapp",
                "abi": 1,
                "install_path": "/apps/demo",
                "entrypoint": "bin/demo",
                "permissions": [],
                "dependencies": [],
            }
            (source / "manifest.json").write_text(json.dumps(manifest))
            packages = root / "packages"
            archive_path = packages / "demo-1.2.3.xora"
            repo.pack_source(source, archive_path)
            index = repo.index_repository(packages)
            self.assertEqual(index["apps"][0]["name"], "demo")
            self.assertEqual(index["apps"][0]["sha256"], repo.sha256(archive_path.read_bytes()))

    def test_signature_index_and_install(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            private_key = root / "repo-private.pem"
            public_key = root / "repo-public.pem"
            index_path = root / "index.json"
            signature_path = root / "index.sig"
            archive_path = root / "demo-1.2.3.xora"
            install_root = root / "target"
            install_root.mkdir()
            archive_data = make_archive()
            archive_path.write_bytes(archive_data)
            manifest, _ = repo.archive_manifest(archive_data)
            index = {
                "format": repo.INDEX_FORMAT,
                "apps": [{
                    **{field: manifest[field] for field in (
                        "name", "version", "description", "category", "runtime",
                        "abi", "install_path", "entrypoint", "permissions", "dependencies")},
                    "archive": archive_path.name,
                    "sha256": repo.sha256(archive_data),
                    "size": len(archive_data),
                }],
            }
            index_data = repo.canonical_json(index)
            index_path.write_bytes(index_data)
            subprocess.run(
                ["openssl", "genpkey", "-algorithm", "ED25519", "-out", str(private_key)],
                check=True, capture_output=True,
            )
            subprocess.run(
                ["openssl", "pkey", "-in", str(private_key), "-pubout", "-out", str(public_key)],
                check=True, capture_output=True,
            )
            repo.sign_index(index_path, private_key, signature_path)
            signature = signature_path.read_bytes()
            repo.verify_signature(index_data, signature, public_key)
            with self.assertRaises(repo.RepoError):
                repo.verify_signature(index_data + b" ", signature, public_key)
            repo.install_package(repo.load_index(index_data), archive_data, install_root, "demo")
            installed = install_root / "apps/demo/bin/demo"
            self.assertEqual(installed.read_bytes(), b"demo executable")
            self.assertTrue(os.stat(installed).st_mode & 0o111)
            state = json.loads((install_root / "var/lib/zircon/apps.json").read_text())
            self.assertEqual(state["apps"][0]["version"], "1.2.3")
            repo.remove_package(install_root, "demo")
            self.assertFalse((install_root / "apps/demo").exists())
            self.assertEqual(json.loads((install_root / "var/lib/zircon/apps.json").read_text())["apps"], [])

    def test_dependency_install_and_remove_order(self):
        base_archive = make_archive(name="base")
        child_archive = make_archive(name="child", dependencies=["base"])
        index = repo.load_index(repo.canonical_json({
            "format": repo.INDEX_FORMAT,
            "apps": [index_entry(base_archive, "base.xora"),
                     index_entry(child_archive, "child.xora")],
        }))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaises(repo.RepoError):
                repo.install_package(index, child_archive, root, "child")
            repo.install_package(index, base_archive, root, "base")
            repo.install_package(index, child_archive, root, "child")
            with self.assertRaises(repo.RepoError):
                repo.remove_package(root, "base")
            repo.remove_package(root, "child")
            repo.remove_package(root, "base")

    def test_index_digest_mismatch_does_not_replace_existing_install(self):
        archive_data = make_archive()
        manifest, _ = repo.archive_manifest(archive_data)
        index = {
            "format": repo.INDEX_FORMAT,
            "apps": [{
                **{field: manifest[field] for field in (
                    "name", "version", "description", "category", "runtime",
                        "abi", "install_path", "entrypoint", "permissions", "dependencies")},
                "archive": "demo.xora",
                "sha256": "0" * 64,
                "size": len(archive_data),
            }],
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            installed = root / "apps/demo"
            installed.mkdir(parents=True)
            sentinel = installed / "keep.txt"
            sentinel.write_text("old")
            with self.assertRaises(repo.RepoError):
                repo.install_package(index, archive_data, root, "demo")
            self.assertEqual(sentinel.read_text(), "old")

    def test_repository_transport_requires_https(self):
        with self.assertRaises(repo.RepoError):
            repo.fetch_https("http://example.invalid/index.json", 10)


if __name__ == "__main__":
    unittest.main()
