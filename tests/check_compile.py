#!/usr/bin/env python3
"""Compile only module objects and discover permanent test_*.cpp regressions; no core configuration/build."""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--core', type=pathlib.Path, required=True)
parser.add_argument('--headers', type=pathlib.Path, help='Optional extracted system development headers')
parser.add_argument('--cxx', default='g++')
args = parser.parse_args()
module = pathlib.Path(__file__).resolve().parents[1]
core = args.core.resolve()
includes = sorted({str(p.parent) for p in (core / 'src').rglob('*.h')})
includes += [str(core / p) for p in ['deps/fmt/include', 'deps/g3dlite/include',
                                   'deps/recastnavigation/Detour/Include', 'deps/SFMT']]
if args.headers:
    includes += [str(args.headers.resolve()), str(args.headers.resolve() / 'mysql')]
flags = ['-std=gnu++20', '-Wall', '-Wextra', '-Werror', '-DBOOST_BIND_GLOBAL_PLACEHOLDERS']
flags += ['-I' + p for p in includes] + ['-I' + str(module / 'src')]
with tempfile.TemporaryDirectory(prefix='item-scaling-check-') as directory:
    out = pathlib.Path(directory)
    for source in sorted((module / 'src').glob('*.cpp')):
        subprocess.run([args.cxx, *flags, '-c', str(source), '-o', str(out / (source.stem + '.o'))], check=True)
        print('PASS compile:', source.name, flush=True)
    subprocess.run(['ld', '-r', *map(str, out.glob('*.o')), '-o', str(out / 'combined.o')], check=True)
    symbols = subprocess.check_output(['nm', '-uC', str(out / 'combined.o')], text=True)
    unresolved = [line for line in symbols.splitlines() if 'ItemScaling' in line]
    if unresolved:
        raise AssertionError('Unresolved module symbols: ' + '\n'.join(unresolved))
    for source in sorted((module / 'tests').glob('test_*.cpp')):
        target = out / source.stem
        extra_flags, extra_sources, arguments = [], [], []
        if source.stem == 'test_startup_identity':
            # Exercise the real core DBC file/overlay loaders with an isolated numeric DB transport.
            extra_flags = ['-I' + str(module / 'tests/dbc_stubs'), '-DFMT_HEADER_ONLY']
            extra_sources = [str(core / p) for p in [
                'src/common/DataStores/DBCFileLoader.cpp',
                'src/server/shared/DataStores/DBCStore.cpp',
                'src/server/shared/DataStores/DBCDatabaseLoader.cpp']]
            arguments = [str(out)]
        subprocess.run([args.cxx, *extra_flags, *flags, str(source), *extra_sources, '-o', str(target)], check=True)
        subprocess.run([str(target), *arguments], check=True)
        print('PASS regression:', source.name, flush=True)
print('PASS: module object compilation, module symbol resolution, and C++ regressions')
