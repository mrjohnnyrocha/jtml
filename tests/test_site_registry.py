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
