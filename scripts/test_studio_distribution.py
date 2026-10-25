#!/usr/bin/env python3
"""Exercise Studio from a clean CLI archive and CMake install, without repo fallback."""
import argparse
import contextlib
import hashlib
import json
import os
import re
from pathlib import Path
import socket
import shutil
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


@contextlib.contextmanager
def studio(binary, cwd, home):
    # Studio also binds a WebSocket listener at HTTP port + 80.
    for _ in range(100):
        with socket.socket() as http, socket.socket() as ws:
            http.bind(('127.0.0.1', 0))
            port = http.getsockname()[1]
            if port > 65455:
                continue
            try:
                ws.bind(('127.0.0.1', port + 80))
            except OSError:
                continue
            break
    else:
        raise RuntimeError('No free Studio port pair')
    with tempfile.TemporaryFile(mode='w+') as log:
        process = subprocess.Popen([str(binary), 'studio', '--port', str(port)],
                                   cwd=cwd, env=clean_environment(home),
                                   stdout=log, stderr=log)
        # Do not let a host proxy intercept local integration requests.
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

        def api(path, payload=None):
            data = None if payload is None else json.dumps(payload).encode()
            request = urllib.request.Request(f'http://127.0.0.1:{port}{path}', data=data,
                                             headers={'Content-Type': 'application/json'})
            with opener.open(request, timeout=10) as response:
                return json.load(response)

        try:
            for _ in range(100):
                try:
                    api('/api/studio/samples')
                    break
                except (urllib.error.URLError, TimeoutError):
                    if process.poll() is not None:
                        break
                    time.sleep(.1)
            else:
                raise RuntimeError('Studio readiness timed out')
            if process.poll() is not None:
                log.seek(0)
                raise RuntimeError(log.read())
            yield api
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)


ENDPOINTS = ('/api/studio/samples', '/api/studio/reference', '/api/studio/sidebar',
             '/api/lessons', '/api/docs')


def snapshot(api):
    return {path: api(path) for path in ENDPOINTS}


def counts(data):
    return {path: len(value) for path, value in data.items()}


def verify_loading(api, expected):
    assert snapshot(api) == expected
    for sample in expected['/api/studio/samples']:
        result = api('/api/format', {'code': sample['code']})
        assert result['ok'] and result['code'].strip(), (sample['name'], result)
    counter = next(s for s in expected['/api/studio/samples'] if s['name'] == 'counter.jtml')
    result = api('/api/run', {'code': counter['code']})
    assert result['ok'] and 'Counter' in result['html'], result
    for lesson in expected['/api/lessons']:
        assert api('/api/lesson/' + lesson['slug'])['code'].strip()
    for doc in expected['/api/docs']:
        assert api('/api/doc/' + doc['slug'])['prose'].strip()


def notices(source, artifact):
    for file in [Path('LICENSE'), Path('THIRD_PARTY_NOTICES.md'),
                 *[p.relative_to(source) for p in (source / 'third_party/licenses').glob('*')]]:
        assert (artifact / file).read_bytes() == (source / file).read_bytes(), file


def clean_environment(home):
    return {'HOME': str(home), 'PATH': os.environ.get('PATH', '/usr/bin:/bin'),
            'LC_ALL': 'C', 'LANG': 'C'}


def verify_commands(prefix, data, expected_version, cwd, home):
    binary = prefix / 'bin/jtml'
    cwd = cwd / ('commands-' + prefix.name)
    cwd.mkdir()
    home = home / prefix.name
    home.mkdir()
    env = clean_environment(home)

    def run(*args):
        return subprocess.run([str(binary), *map(str, args)], cwd=cwd, env=env,
                              check=True, capture_output=True, text=True).stdout

    assert run('--version').strip() == 'jtml ' + expected_version
    assert 'friendly_counter.jtml' in run('examples')
    check = json.loads(run('check', data / 'examples/friendly_counter.jtml', '--json'))
    assert check['ok'] and not check['diagnostics'], check
    output = cwd / ('browser-' + prefix.name)
    run('build', data / 'examples/friendly_counter.jtml', '--target', 'browser', '--out', output)
    assert {p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()} == {
        'index.html', 'jtml-runtime.js', 'components/index.js', 'app.js'}
    for file in output.rglob('*.js'):
        text = file.read_text()
        assert 'new Function' not in text and not re.search(r'\beval\s*\(', text), file
    libraries = [p for p in prefix.rglob('libjtml.*') if p.suffix in ('.dylib', '.so')]
    assert len(libraries) == 1, libraries
    library_dir = libraries[0].parent
    # Compile against the shipped public header; load only the shipped library,
    # with no source/build include paths or ambient loader overrides.
    client = cwd / ('client-' + prefix.name + '.c')
    client.write_text(r'''#include <jtml/c_api.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    char *html = NULL, *error = NULL;
    if (argc != 2 || strcmp(jtml_abi_version(), argv[1])) return 1;
    if (!jtml_render("jtml 2\npage\n  h1 \"Installed C API\"\n", &html, &error)) return 2;
    int valid = html && strstr(html, "Installed C API");
    jtml_free(html);
    jtml_free(error);
    return valid ? 0 : 3;
}
''')
    executable = cwd / ('client-' + prefix.name)
    subprocess.run(['cc', str(client), '-I' + str(prefix / 'include'), '-L' + str(library_dir),
                    '-ljtml', '-Wl,-rpath,' + str(library_dir), '-o', str(executable)],
                   cwd=cwd, env=env, check=True, capture_output=True, text=True)
    subprocess.run([str(executable), expected_version], cwd=cwd, env=env,
                   check=True, capture_output=True, text=True)
    print('PASS clean-room CLI version/check/browser build, examples and compiled C API:', prefix.name,
          flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--install-prefix', type=Path)
    parser.add_argument('--cpack-archive', type=Path)
    parser.add_argument('--expect-missing', action='store_true', help='Reproduce the pre-fix archive defect')
    args = parser.parse_args()
    source = args.source.resolve()
    build = (args.build_dir or source / 'build').resolve()
    expected_version = (build / 'jtml-version.txt').read_text().strip()
    with tempfile.TemporaryDirectory(prefix='jtml-studio-distribution-') as temporary:
        temp = Path(temporary)
        home, unrelated, extracted = (temp / name for name in ('home', 'unrelated', 'extracted'))
        for directory in (home, unrelated, extracted):
            directory.mkdir()
        subprocess.run(['tar', '-xzf', str(args.archive.resolve()), '-C', str(extracted)], check=True)
        roots = list(extracted.iterdir())
        assert len(roots) == 1
        archive = roots[0]
        metadata = json.loads((archive / 'ARTIFACT.json').read_text()) if not args.expect_missing else {}
        if not args.expect_missing:
            assert metadata['version'] == expected_version, metadata
        with studio(build / 'jtml', source, home) as api:
            expected = snapshot(api)
        manifest = json.loads((source / 'studio/samples/manifest.json').read_text())
        assert len(expected['/api/studio/samples']) == len(manifest) > 0
        for sample, entry in zip(expected['/api/studio/samples'], manifest):
            assert sample['code'] == (source / 'studio/samples' / entry['file']).read_text()
        assert expected['/api/studio/reference'] == json.loads(
            (source / 'studio/reference/catalog.json').read_text())
        assert expected['/api/studio/sidebar'] == json.loads(
            (source / 'studio/sidebar/catalog.json').read_text())
        assert expected['/api/lessons'] and expected['/api/docs']
        print('Source:', counts(expected), flush=True)
        with studio(archive / 'bin/jtml', archive, home) as api:
            actual = snapshot(api)
            print('Extracted archive:', counts(actual), flush=True)
            if args.expect_missing:
                assert actual['/api/studio/samples'] == []
                assert actual['/api/studio/reference'] == []
                assert actual['/api/studio/sidebar'] == {}
                assert actual['/api/lessons'] == expected['/api/lessons']
                assert actual['/api/docs'] == expected['/api/docs']
                print('PASS: pre-fix missing Studio resources reproduced')
                return
            verify_loading(api, expected)
        notices(source, archive)
        checksummed = set()
        for line in (archive / 'SHA256SUMS').read_text().splitlines():
            digest, file = line.split('  ', 1)
            assert hashlib.sha256((archive / file).read_bytes()).hexdigest() == digest, file
            checksummed.add(Path(file))
        file_manifest = (archive / 'MANIFEST.txt').read_text().splitlines()
        for file in (source / 'studio').rglob('*'):
            if file.is_file():
                relative = file.relative_to(source)
                assert (archive / relative).read_bytes() == file.read_bytes()
                assert relative in checksummed, relative
                assert str(relative) in file_manifest, relative
        # Move after extraction, removing the original path completely.
        relocated = temp / 'relocated archive'
        archive.rename(relocated)
        archive = relocated
        # Cwd must not override a packaged catalog.
        poison = unrelated / 'studio/samples'
        poison.mkdir(parents=True)
        (poison / 'manifest.json').write_text('[]')
        verify_commands(archive, archive, expected_version, unrelated, home)
        with studio(archive / 'bin/jtml', unrelated, home) as api:
            verify_loading(api, expected)
        link = temp / 'linked-jtml'
        link.symlink_to(archive / 'bin/jtml')
        with studio(link, unrelated, home) as api:
            verify_loading(api, expected)
        print('PASS: archive APIs, all sample sources/formatting, Counter execution, lessons/docs, notices/checksums/manifest')
        if not args.install_prefix:
            args.install_prefix = temp / 'install'
            subprocess.run(['cmake', '--install', str(build), '--prefix',
                            str(args.install_prefix)], check=True, stdout=subprocess.DEVNULL)
        if args.install_prefix:
            prefix = args.install_prefix.resolve()
            data = prefix / 'share/jtml'
            installed_expected = expected
            with studio(prefix / 'bin/jtml', data, home) as api:
                verify_loading(api, installed_expected)
                print('CMake install:', counts(snapshot(api)))
            notices(source, prefix / 'share/doc/jtml')
            with studio(prefix / 'bin/jtml', prefix / 'share/doc/jtml', home) as api:
                assert api('/api/docs') == expected['/api/docs']
                for doc in expected['/api/docs']:
                    assert api('/api/doc/' + doc['slug'])['prose'].strip()
            installed_manifest = {Path(p).resolve() for p in
                                  (build / 'install_manifest.txt').read_text().splitlines()}
            for file in (source / 'studio').rglob('*'):
                if file.is_file():
                    installed = data / file.relative_to(source)
                    assert installed.read_bytes() == file.read_bytes()
                    assert installed.resolve() in installed_manifest, installed
            # Never move a caller-owned installation. Relocate a temporary copy.
            moved = temp / 'relocated install'
            shutil.copytree(prefix, moved)
            verify_commands(moved, moved / 'share/jtml', expected_version, unrelated, home)
            with studio(moved / 'bin/jtml', unrelated, home) as api:
                verify_loading(api, expected)
            print('PASS: install sample loading, catalogs, notices and install manifest')
        if args.cpack_archive:
            cpack = temp / 'cpack'
            cpack.mkdir()
            subprocess.run(['tar', '-xzf', str(args.cpack_archive.resolve()), '-C', str(cpack)], check=True)
            roots = list(cpack.iterdir())
            assert len(roots) == 1
            moved = temp / 'relocated cpack'
            roots[0].rename(moved)
            verify_commands(moved, moved / 'share/jtml', expected_version, unrelated, home)
            with studio(moved / 'bin/jtml', unrelated, home) as api:
                verify_loading(api, expected)
            notices(source, moved / 'share/doc/jtml')
        print('PASS: executable-relative archive/install resources, relocation, cwd isolation and symlink launch')


if __name__ == '__main__':
    main()
