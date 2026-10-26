# RC.1 macOS 14 compatibility correction

## Reproduced release blocker

Run [40150110121](https://github.com/mrjohnnyrocha/jtml/actions/runs/40150110121),
source `3542dc8d6cb843faef68a2bd5af65a9a8507b2aa`, passed hosted Ubuntu/macOS 15
checks. Its downloaded macOS review payload passes SHA256SUMS. On physical macOS
14.4.1 (23E224), arm64, the CLI aborts before `--version` with missing
`__ZNSt13exception_ptr31__from_native_exception_pointerEPv` in system libc++.

Both `bin/jtml` and `lib/libjtml.dylib` have LC_BUILD_VERSION minos 15.0, SDK 15.5,
and undefined references to that symbol and `___cxa_init_primary_exception`.
Both depend on `/usr/lib/libc++.1.dylib` and `/usr/lib/libSystem.B.dylib`.
The builder was Apple Clang 17.0.0 from Xcode 16.4 on macOS 15 arm64. No minimum
OS was set; the toolchain selected its newer default. Valid checksums establish
byte integrity, not backward compatibility. No HTTP/server or source change fixes
this process-startup failure.

## Narrow correction and independent validation

The existing packager passes `-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0` explicitly on
macOS before CMake's project/toolchain initialization, overriding a stale cache.
The RC wrapper and manual workflow enforce that target; CPack uses the same build
and installed binaries. Linux packaging, version identity, source provenance,
notices, checksums and existing installation acceptance tests remain in place.

[CMake documents](https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_DEPLOYMENT_TARGET.html)
that this becomes the compiler deployment flag. This must also select compatible
libc++ header paths: [LLVM's availability mechanism](https://libcxx.llvm.org/VendorDocumentation.html)
exists because newer headers can otherwise reference runtime symbols absent on
older systems. Lowering the Mach-O header after linking is not a fix.

```sh
python3 tests/test_macos_distribution.py
python3 tests/test_prepare_rc.py
# Exact clean source, on the existing macos-15 hosted runner:
MACOSX_DEPLOYMENT_TARGET=14.0 python3 scripts/prepare_rc.py --source-revision <SHA>
# Both actual extracted binaries, never development substitutes:
python3 scripts/check_macos_distribution.py /path/to/extracted/release
xcrun vtool -show-build /path/to/extracted/release/bin/jtml
xcrun vtool -show-build /path/to/extracted/release/lib/libjtml.dylib
otool -L /path/to/extracted/release/bin/jtml
nm -u /path/to/extracted/release/bin/jtml
```

The guard rejects missing/ambiguous/non-macOS metadata, wrong minimum OS or
architecture, the two observed incompatible imports even with a lowered header,
and unshipped non-system dylibs. It records all undefined symbols, SDK/dependencies
and hashes in MACOS_COMPATIBILITY.json. It is a regression guard, not a complete
symbol-availability database. macOS 15 CI alone cannot certify macOS 14.

Final acceptance must download the regenerated exact-commit bundle **and** CPack
archive to physical macOS 14.4.1, verify checksums and run the existing distribution
checks there (clean HOME/unrelated cwd, CLI/version/check/browser build, compiled
C caller, relocated Studio/catalogs). Run Counter 0 → Add one 1 → Reset 0 in a real
browser and the existing production-reference suite with clean browser exit.
Eager binding (`DYLD_BIND_AT_LAUNCH=1` for CLI and `RTLD_NOW` for C API) additionally
checks imported symbols against the older host's actual dynamic libraries.
Record exact hosted/local results and archive hashes in the corrective PR.
If the newer toolchain still emits unavailable symbols, this release remains
blocked until a compatible Apple toolchain/build environment is validated.

The earlier local Apple Clang 15/macOS 14.4.1 result remains historical evidence
for its own locally built artifacts only. It does not certify the failed hosted
artifact or future SDK/toolchain combinations. No signing, notarization, universal
binary, older macOS or Intel support is implied.
