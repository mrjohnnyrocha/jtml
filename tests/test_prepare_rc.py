#!/usr/bin/env python3
"""Fail-closed controls for the RC wrapper; no compiler/build/network needed."""
import importlib.util
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('prepare_rc', Path(__file__).resolve().parents[1] / 'scripts/prepare_rc.py')
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)


class RcGuards(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.command('init', '-q')
        (self.root / '.gitignore').write_text('dist/\nbuild/\n')
        (self.root / 'source.txt').write_text('reviewed source\n')
        self.command('add', '.')
        self.command('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                     '-c', 'commit.gpgsign=false', 'commit', '-qm', 'Fixture')
        self.sha = rc.git(self.root, 'rev-parse', 'HEAD')

    def command(self, *args):
        subprocess.run(['git', '-C', str(self.root), *args], check=True, capture_output=True)

    def test_exact_clean_sha_and_ignored_outputs_are_accepted(self):
        (self.root / 'dist').mkdir()
        (self.root / 'dist/package').write_text('generated output')
        rc.require_clean_revision(self.root, self.sha.upper())

    def test_moving_ref_is_rejected(self):
        with self.assertRaises(ValueError):
            rc.require_clean_revision(self.root, 'main')

    def test_different_exact_commit_is_rejected(self):
        with self.assertRaises(ValueError):
            rc.require_clean_revision(self.root, '0' * 40)

    def test_modified_source_is_rejected(self):
        (self.root / 'source.txt').write_text('unreviewed change')
        with self.assertRaises(ValueError):
            rc.require_clean_revision(self.root, self.sha)

    def test_untracked_payload_is_rejected(self):
        (self.root / 'unreviewed.txt').write_text('unreviewed payload')
        with self.assertRaises(ValueError):
            rc.require_clean_revision(self.root, self.sha)

    def test_rc_rejects_newer_macos_target_before_build(self):
        with patch.object(rc.platform, 'system', return_value='Darwin'), \
             patch.object(rc.platform, 'machine', return_value='arm64'), \
             patch.dict(rc.os.environ, {'MACOSX_DEPLOYMENT_TARGET': '15.0'}):
            with self.assertRaisesRegex(ValueError, 'must be 14.0'):
                rc.prepare(self.root, self.sha, self.root / 'build')

    def metadata(self):
        return dict(version='0.3.0-rc.1', sourceRevision=self.sha, sourceDirty=False,
                    platform='linux', architecture='x86_64', sourceDateEpoch=123)

    def test_actual_rc_identity_is_required(self):
        rc.validate_metadata(self.metadata(), self.sha, 'linux', 'x86_64', 123)
        for key, value in [('version', '0.3.0-dev'), ('sourceDirty', True),
                           ('sourceRevision', '0' * 40), ('platform', 'darwin'),
                           ('architecture', 'arm64'), ('sourceDateEpoch', 0)]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                rc.validate_metadata({**self.metadata(), key: value}, self.sha, 'linux', 'x86_64', 123)


if __name__ == '__main__':
    unittest.main()
