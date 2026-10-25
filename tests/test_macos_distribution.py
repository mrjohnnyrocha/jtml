#!/usr/bin/env python3
"""Independent negative oracles for packaged Mach-O release guards; runs on Linux too."""
import importlib.util
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch
import unittest

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('macho', Path(__file__).resolve().parents[1] /
                                              'scripts/check_macos_distribution.py')
macho = importlib.util.module_from_spec(spec)
spec.loader.exec_module(macho)


class MacOSDistributionGuards(unittest.TestCase):
    def test_both_packaged_binaries_are_audited(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ('bin/jtml', 'lib/libjtml.dylib'):
                file = root / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(b'Mach-O fixture')

            def command(args, **kwargs):
                tool, _, file = args
                if tool == 'lipo':
                    return 'arm64\n'
                if tool == 'nm':
                    return '_malloc\n'
                if args[1] == '-L':
                    return str(file) + ':\n /usr/lib/libc++.1.dylib (compatibility version 1.0.0)\n'
                minimum = '15.0' if self.bad_c_api and str(file).endswith('.dylib') else '14.0'
                return 'cmd LC_BUILD_VERSION\n platform 1\n minos ' + minimum + '\n sdk 15.5\n'

            with patch.object(macho.subprocess, 'check_output', side_effect=command):
                self.bad_c_api = False
                self.assertEqual([item['file'] for item in macho.audit(root)['binaries']],
                                 ['bin/jtml', 'lib/libjtml.dylib'])
                self.bad_c_api = True
                with self.assertRaises(ValueError):
                    macho.audit(root)
            with patch.object(macho.subprocess, 'check_output', return_value='x86_64\n'):
                with self.assertRaises(ValueError):
                    macho.audit(root)

    def test_sdk_is_not_minimum_os(self):
        text = 'Load command 11\n cmd LC_BUILD_VERSION\n platform 1\n minos 14.0\n sdk 15.5\n'
        self.assertEqual(macho.validate_load_commands(text), dict(minimumOS='14.0', sdk='15.5'))
        with self.assertRaises(ValueError):
            macho.validate_load_commands(text.replace('minos 14.0', 'minos 15.0'))

    def test_missing_ambiguous_or_non_macos_metadata_rejected(self):
        valid = 'cmd LC_BUILD_VERSION\n platform 1\n minos 14.0\n sdk 15.5\n'
        for text in ('', valid + 'Load command 12\n' + valid,
                     valid.replace('platform 1', 'platform 2'), valid.replace('sdk 15.5', '')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                macho.validate_load_commands(text)

    def test_legacy_minimum_os_command(self):
        self.assertEqual(macho.validate_load_commands(
            'cmd LC_VERSION_MIN_MACOSX\n version 14.0\n sdk 14.4')['minimumOS'], '14.0')

    def test_lowered_metadata_cannot_hide_bad_libcxx_imports(self):
        # A patched minos header alone must not let the reported regression pass.
        for symbol in ('__ZNSt13exception_ptr31__from_native_exception_pointerEPv',
                       '___cxa_init_primary_exception'):
            with self.subTest(symbol=symbol), self.assertRaises(ValueError):
                macho.validate_imports('_malloc\n' + symbol + '\n')
        macho.validate_imports('__ZNSt13exception_ptrD1Ev\n_malloc\n')

    def test_unshipped_runtime_dependencies_rejected(self):
        valid = 'jtml:\n /usr/lib/libc++.1.dylib (compatibility version 1.0.0)\n'
        self.assertEqual(macho.validate_libraries(valid), ['/usr/lib/libc++.1.dylib'])
        for path in ('/opt/homebrew/lib/libc++.1.dylib', '@rpath/libother.dylib',
                     '/Users/runner/build/libjtml.dylib'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                macho.validate_libraries(valid + ' ' + path + ' (compatibility version 1.0.0)\n')
        with self.assertRaises(ValueError):
            macho.validate_libraries('jtml:\n')


if __name__ == '__main__':
    unittest.main()
