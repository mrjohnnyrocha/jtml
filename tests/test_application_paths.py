"""Regression checks for the real OpsPulse files, not a substitute module tree."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CLI = Path(os.environ.get('JTML_BIN', ROOT / 'build/jtml')).resolve()


class ApplicationPaths(unittest.TestCase):
    def check(self, source):
        result = subprocess.run([str(CLI), 'check', str(source)], capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_real_dashboard_transitive_imports(self):
        self.check(ROOT / 'demos/opspulse/pages/dashboard.jtml')

    def test_named_public_exports(self):
        # A temporary consumer imports real modules; definitions are not copied.
        modules = {
            'stores/app-state.jtml': ['appState'],
            'stores/filter-state.jtml': ['filterState'],
            'stores/ops-data.jtml': ['opsData'],
            'components/layouts/shell.jtml': ['Shell'],
            'components/ui/app-header.jtml': ['AppHeader'],
            'components/ui/app-sidebar.jtml': ['AppSidebar'],
            'components/ui/filter-panel.jtml': ['FilterPanel'],
            'components/ui/common.jtml': ['AlertBar', 'MetricCard', 'LoadingBlock', 'ErrorBlock', 'EmptyBlock'],
            'pages/dashboard.jtml': ['Dashboard'],
        }
        with tempfile.TemporaryDirectory() as directory:
            for module, names in modules.items():
                with self.subTest(module=module):
                    source = Path(directory) / 'consumer.jtml'
                    target = ROOT / 'demos/opspulse' / module
                    source.write_text('jtml 2\nuse { ' + ', '.join(names) + ' } from "' + str(target) + '"\npage\n  text "Import contract"\n')
                    self.check(source)

    def test_current_download_and_pinned_site_compiler_are_distinct(self):
        for file in ('README.md', 'site/index.jtml', 'site/deploy.html'):
            text = (ROOT / file).read_text()
            self.assertIn('releases/tag/v0.3.0-rc.3', text, file)
            self.assertNotIn('releases/tag/v0.3.0-rc.2', text, file)
        workflow = (ROOT / '.github/workflows/site-pages.yml').read_text()
        self.assertNotIn('releases/download/v0.3.0-rc.2/', workflow)
        self.assertIn('scripts/build_site.sh dist/site', workflow)
        pin = json.loads((ROOT / 'site/compiler.json').read_text())
        self.assertRegex(pin['revision'], r'^[0-9a-f]{40}$')
        self.assertEqual(pin['version'], 'jtml 0.3.0-dev')
        self.check(ROOT / 'site/index.jtml')


if __name__ == '__main__':
    unittest.main()
