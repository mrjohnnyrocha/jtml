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
            if page['target'] != 'browser':
                continue
            bundle = self.root / page['id']
            bundle.mkdir()
            (bundle / 'jtml-runtime.js').write_bytes(b'compiler runtime\n')
            bundles[page['output']] = bundle
        builder.verify_shared_runtime(bundles)
        changed = bundles['reference.html'] / 'jtml-runtime.js'
        changed.write_bytes(b'compiler Runtime\n')  # Same length, different bytes.
        with self.assertRaisesRegex(ValueError, 'runtimes differ: reference.html'):
            builder.verify_shared_runtime(bundles)
        self.assertEqual(changed.read_bytes(), b'compiler Runtime\n')
        self.assertEqual(len(list(self.root.rglob('jtml-runtime.js'))), 2)

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
            assets = builder.ASSETS if args[-1] == 'generated' else builder.page_assets(next(p for p in self.registry['pages'] if p['id'] == args[-1]))
            bundle.mkdir(parents=True)
            if 'components/index.js' in assets:
                (bundle / 'components').mkdir()
            for name in assets:
                (bundle / name).write_text('<html><head><title></title></head><body></body></html>' if name == 'index.html' else '// generated')
            if args[-1] == 'reference':
                (bundle / 'jtml-runtime.js').write_text('// different')

        with patch.object(builder.subprocess, 'check_output', return_value='jtml test'), patch.object(builder.subprocess, 'run', side_effect=compile_fixture):
            with self.assertRaisesRegex(ValueError, 'runtimes differ: reference.html'):
                builder.build(output, str(compiler), 'https://example.test/jtml/')
        self.assertEqual((output / 'keep.txt').read_text(), 'previous accepted site')
        originals = output.with_name('site.failed-compiler-output')
        self.assertEqual(len(list(originals.rglob('jtml-runtime.js'))), 2)
        self.assertEqual((originals / 'reference/jtml-runtime.js').read_text(), '// different')

    def test_late_static_rejection_never_falls_back_or_replaces_accepted_output(self):
        output = self.root / 'dist/site'
        output.mkdir(parents=True)
        (output / 'keep.txt').write_text('accepted site')
        originals = output.with_name('site.compiler-output')
        originals.mkdir()
        (originals / 'keep.txt').write_text('accepted originals')
        (self.site / 'shared').mkdir()
        (self.root / 'examples').mkdir()
        (self.root / 'examples/friendly_counter.jtml').write_text('jtml 2')
        compiler = self.root / 'compiler'
        compiler.write_text('failure injection compiler')
        targets = []
        def compile_fixture(args, cwd, check):
            targets.append((args[-1], args[args.index('--target') + 1]))
            if args[-1] == 'security':
                raise builder.subprocess.CalledProcessError(1, args)
            page = next((p for p in self.registry['pages'] if p['id'] == args[-1]), None)
            assets = builder.page_assets(page) if page else builder.ASSETS
            bundle = Path(cwd) / args[-1]
            bundle.mkdir()
            for name in assets:
                target = bundle / name
                target.parent.mkdir(exist_ok=True)
                target.write_text('<head></head>' if name == 'index.html' else '// generated')
        with patch.object(builder.subprocess, 'check_output', return_value='jtml test'), patch.object(builder.subprocess, 'run', side_effect=compile_fixture):
            with self.assertRaises(builder.subprocess.CalledProcessError):
                builder.build(output, str(compiler), 'https://example.test/jtml/')
        self.assertEqual(targets, [('generated', 'browser'), ('homepage', 'browser'), ('reference', 'browser'), ('examples', 'static'), ('tools', 'static'), ('security', 'static')])
        self.assertEqual((output / 'keep.txt').read_text(), 'accepted site')
        self.assertEqual((originals / 'keep.txt').read_text(), 'accepted originals')
        self.assertFalse((output / 'index.html').exists())

    def test_shared_runtime_path_cannot_escape_or_collide(self):
        for value in ('../jtml-runtime.js', 'app.js', 'reference-assets/jtml-runtime.js'):
            self.registry['sharedRuntime'] = value
            self.write()
            with self.assertRaisesRegex(ValueError, 'Shared runtime'):
                builder.load_page_registry()

    def test_exact_mixed_targets_and_no_static_bundle_directory(self):
        self.assertEqual({p['output'] for p in self.registry['pages'] if p['target'] == 'browser'}, {'index.html', 'reference.html'})
        self.registry['pages'][2]['target'] = 'browser'
        self.registry['pages'][2]['assetDirectory'] = 'examples-assets'
        self.write()
        with self.assertRaisesRegex(ValueError, 'Only Homepage'):
            builder.load_page_registry()
        self.registry['pages'][2]['target'] = 'static'
        self.write()
        with self.assertRaisesRegex(ValueError, 'Static pages'):
            builder.load_page_registry()

    def test_missing_browser_runtime_cannot_bypass_equality(self):
        with self.assertRaisesRegex(ValueError, 'both browser'):
            builder.verify_shared_runtime({})

    def test_failure_keeps_previously_accepted_compiler_originals(self):
        output = self.root / 'dist/site'
        output.mkdir(parents=True)
        (output / 'keep.txt').write_text('accepted site')
        originals = output.with_name('site.compiler-output')
        originals.mkdir()
        (originals / 'keep.txt').write_text('accepted compiler originals')
        with patch.object(builder, '_build', side_effect=RuntimeError('static compiler rejected state')):
            with self.assertRaisesRegex(RuntimeError, 'rejected state'):
                builder.build(output, '/unused', 'https://example.test/jtml/')
        self.assertEqual((output / 'keep.txt').read_text(), 'accepted site')
        self.assertEqual((originals / 'keep.txt').read_text(), 'accepted compiler originals')

    def test_publication_failure_rolls_back_both_directories(self):
        output = self.root / 'dist/site'
        output.mkdir(parents=True)
        (output / 'keep.txt').write_text('accepted site')
        originals = output.with_name('site.compiler-output')
        originals.mkdir()
        (originals / 'keep.txt').write_text('accepted compiler originals')
        def prepared(staged, compiler, url):
            staged.mkdir()
            (staged / 'new.txt').write_text('new site')
            compiled = staged.with_name(staged.name + '.compiler-output')
            compiled.mkdir()
            (compiled / 'new.txt').write_text('new compiler originals')
        original_replace = builder.os.replace
        def fail_second_publish(source, target):
            if Path(source).name == 'site.compiler-output' and Path(target).resolve() == originals.resolve():
                raise OSError('injected provenance publication failure')
            return original_replace(source, target)
        with patch.object(builder, '_build', side_effect=prepared), patch.object(builder.os, 'replace', side_effect=fail_second_publish):
            with self.assertRaisesRegex(OSError, 'publication failure'):
                builder.build(output, '/unused', 'https://example.test/jtml/')
        self.assertEqual(sorted(p.name for p in output.iterdir()), ['keep.txt'])
        self.assertEqual((output / 'keep.txt').read_text(), 'accepted site')
        self.assertEqual((originals / 'keep.txt').read_text(), 'accepted compiler originals')

    def test_bundle_collision_rejected(self):
        self.registry['pages'][1]['assetDirectory'] = '.'
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
