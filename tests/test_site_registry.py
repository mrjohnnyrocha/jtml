"""Page registry collision/path guards, before invoking or altering build output."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('website_builder', ROOT / 'scripts/build_public_site.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class PageRegistry(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.site = self.root / 'site'
        self.site.mkdir()
        self.registry = json.loads((ROOT / 'site/pages.json').read_text())
        for page in self.registry['pages']:
            (self.site / page['entry']).write_text('jtml 2\npage\n  h1 "Test"\n')
            for css in page['css']:
                (self.site / css).touch()
        self.write()
        self.root_patch = patch.object(builder, 'ROOT', self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)

    def write(self):
        (self.site / 'pages.json').write_text(json.dumps(self.registry))

    def test_six_distinct_public_entries(self):
        self.assertEqual(builder.load_page_registry(), self.registry)
        self.assertEqual(len({page['entry'] for page in self.registry['pages']}), 6)

    def test_runtime_sharing_requires_actual_byte_equality(self):
        bundles = {}
        for page in self.registry['pages']:
            bundle = self.root / page['id']
            bundle.mkdir()
            (bundle / 'jtml-runtime.js').write_bytes(b'compiler runtime\n')
            bundles[page['output']] = bundle
        builder.verify_shared_runtime(bundles)
        changed = bundles['deploy.html'] / 'jtml-runtime.js'
        changed.write_bytes(b'compiler Runtime\n')  # Same length, different bytes.
        with self.assertRaisesRegex(ValueError, 'runtimes differ: deploy.html'):
            builder.verify_shared_runtime(bundles)
        self.assertEqual(changed.read_bytes(), b'compiler Runtime\n')
        self.assertEqual(len(list(self.root.rglob('jtml-runtime.js'))), 6)

    def test_build_rejects_mismatch_before_replacing_public_output(self):
        output = self.root / 'dist/site'
        output.mkdir(parents=True)
        (output / 'keep.txt').write_text('previous accepted site')
        (self.site / 'shared').mkdir()
        (self.root / 'examples').mkdir()
        (self.root / 'examples/friendly_counter.jtml').write_text('jtml 2')
        compiler = self.root / 'compiler'
        compiler.write_text('fake compiler for packaging failure injection')

        def compile_fixture(args, cwd, check):
            bundle = Path(cwd) / args[-1]
            (bundle / 'components').mkdir(parents=True)
            for name in builder.ASSETS:
                (bundle / name).write_text('<head></head>' if name == 'index.html' else '// generated')
            if args[-1] == 'security':
                (bundle / 'jtml-runtime.js').write_text('// different')

        with patch.object(builder.subprocess, 'check_output', return_value='jtml test'), patch.object(builder.subprocess, 'run', side_effect=compile_fixture):
            with self.assertRaisesRegex(ValueError, 'runtimes differ: security.html'):
                builder.build(output, str(compiler), 'https://example.test/jtml/')
        self.assertEqual((output / 'keep.txt').read_text(), 'previous accepted site')
        originals = output.with_name('site.compiler-output')
        self.assertEqual(len(list(originals.rglob('jtml-runtime.js'))), 6)
        self.assertEqual((originals / 'security/jtml-runtime.js').read_text(), '// different')

    def test_shared_runtime_path_cannot_escape_or_collide(self):
        for value in ('../jtml-runtime.js', 'app.js', 'reference-assets/jtml-runtime.js'):
            self.registry['sharedRuntime'] = value
            self.write()
            with self.assertRaisesRegex(ValueError, 'Shared runtime'):
                builder.load_page_registry()

    def test_bundle_collision_rejected(self):
        self.registry['pages'][2]['assetDirectory'] = '.'
        self.write()
        with self.assertRaisesRegex(ValueError, 'collision'):
            builder.load_page_registry()

    def test_source_and_bundle_paths_cannot_escape(self):
        for field, value in [('entry', '../index.jtml'), ('assetDirectory', '../assets')]:
            with self.subTest(field=field):
                original = self.registry['pages'][0][field]
                self.registry['pages'][0][field] = value
                self.write()
                with self.assertRaises(ValueError):
                    builder.load_page_registry()
                self.registry['pages'][0][field] = original

    def test_missing_metadata_and_css_rejected(self):
        self.registry['pages'][1].pop('title')
        self.write()
        with self.assertRaisesRegex(ValueError, 'fields'):
            builder.load_page_registry()
        self.registry['pages'][1]['title'] = 'Reference'
        self.registry['pages'][1]['css'] = ['missing.css']
        self.write()
        with self.assertRaisesRegex(ValueError, 'CSS'):
            builder.load_page_registry()

    def test_handwritten_html_cannot_replace_output(self):
        output = self.root / 'dist/site'
        output.mkdir(parents=True)
        marker = output / 'keep.txt'
        marker.write_text('existing build')
        (self.site / 'hidden.html').write_text('<main>Handwritten substitute</main>')
        with self.assertRaisesRegex(ValueError, 'Handwritten page HTML'):
            builder.build(output, '/must-not-execute', 'https://example.test/jtml/')
        self.assertEqual(marker.read_text(), 'existing build')


if __name__ == '__main__':
    unittest.main()
