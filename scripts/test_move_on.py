"""Host-side safety tests for the Zircon Move On transfer tool."""

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile


SCRIPT = Path(__file__).with_name("move_on.py")
SPEC = importlib.util.spec_from_file_location("move_on", SCRIPT)
move_on = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(move_on)


class MoveOnTests(unittest.TestCase):
    def test_round_trip_nested_files_empty_dirs_and_permissions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "zircon-data"
            (source / "Documents" / "empty").mkdir(parents=True)
            document = source / "Documents" / "note.txt"
            document.write_text("Zircon notes\n", encoding="utf-8")
            document.chmod(0o640)
            bundle = root / "backup.moveon"
            destination = root / "codeos-data"

            move_on.export_tree(str(source), str(bundle))
            imported, count = move_on.import_tree(str(bundle), str(destination))

            self.assertEqual(imported, destination)
            self.assertEqual(count, 1)
            self.assertEqual((destination / "Documents" / "note.txt").read_text(), "Zircon notes\n")
            self.assertEqual((destination / "Documents" / "note.txt").stat().st_mode & 0o777, 0o640)
            self.assertTrue((destination / "Documents" / "empty").is_dir())

    def test_round_trip_large_file(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "zircon-data"
            source.mkdir()
            contents = bytes(range(256)) * 8192
            (source / "photo.raw").write_bytes(contents)
            bundle = root / "backup.moveon"
            destination = root / "codeos-data"

            move_on.export_tree(str(source), str(bundle))
            move_on.import_tree(str(bundle), str(destination))

            self.assertEqual((destination / "photo.raw").read_bytes(), contents)

    def test_export_refuses_symlinks_and_existing_bundle(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "link").symlink_to(root)
            bundle = root / "backup.moveon"
            with self.assertRaises(move_on.MoveOnError):
                move_on.export_tree(str(source), str(bundle))
            self.assertFalse(bundle.exists())

            (source / "link").unlink()
            bundle.write_bytes(b"keep")
            with self.assertRaises(move_on.MoveOnError):
                move_on.export_tree(str(source), str(bundle))
            self.assertEqual(bundle.read_bytes(), b"keep")

    def test_import_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            (source / "data.txt").write_text("data", encoding="utf-8")
            bundle = root / "backup.moveon"
            move_on.export_tree(str(source), str(bundle))
            destination = root / "existing"
            destination.mkdir()
            (destination / "keep.txt").write_text("keep", encoding="utf-8")

            with self.assertRaises(move_on.MoveOnError):
                move_on.import_tree(str(bundle), str(destination))
            self.assertEqual((destination / "keep.txt").read_text(), "keep")

    def test_import_rejects_traversal_and_integrity_mismatch(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = root / "unsafe.moveon"
            manifest = {
                "format": move_on.FORMAT,
                "source_system": "Zircon",
                "directories": [],
                "files": [{
                    "path": "../escape.txt",
                    "size": 1,
                    "sha256": hashlib.sha256(b"x").hexdigest(),
                    "mode": 0o644,
                }],
            }
            with zipfile.ZipFile(bundle, "w") as archive:
                archive.writestr("manifest.json", json.dumps(manifest))
                archive.writestr("payload/../escape.txt", b"x")
            with self.assertRaises(move_on.MoveOnError):
                move_on.import_tree(str(bundle), str(root / "destination"))
            self.assertFalse((root / "escape.txt").exists())

    def test_import_detects_corrupt_file_hash_and_cleans_staging_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = root / "corrupt.moveon"
            manifest = {
                "format": move_on.FORMAT,
                "source_system": "Zircon",
                "directories": [],
                "files": [{
                    "path": "note.txt",
                    "size": 4,
                    "sha256": hashlib.sha256(b"good").hexdigest(),
                    "mode": 0o644,
                }],
            }
            with zipfile.ZipFile(bundle, "w") as archive:
                archive.writestr("manifest.json", json.dumps(manifest))
                archive.writestr("payload/note.txt", b"evil")
            destination = root / "destination"

            with self.assertRaises(move_on.MoveOnError):
                move_on.import_tree(str(bundle), str(destination))

            self.assertFalse(destination.exists())
            self.assertEqual(list(root.glob(".move-on-*")), [])


if __name__ == "__main__":
    unittest.main()
