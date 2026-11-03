#!/usr/bin/env python
"""CI script to install Windows dependencies (libxml2 and libiconv)
"""

from __future__ import print_function

import sys, os
import shutil
import fileinput
import subprocess as sp
import re

# gendef.exe is provided by the MSYS2 environment set up in the workflow
gendef = 'C:/msys64/{0}/bin/gendef'.format(os.environ.get('MSYSTEM', 'UCRT64').lower())

def find_msvc_lib():
    """Locate lib.exe of the latest installed MSVC toolset using vswhere"""
    vswhere = os.path.join(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'),
                           'Microsoft Visual Studio', 'Installer', 'vswhere.exe')
    out = sp.check_output([vswhere, '-latest', '-products', '*',
                           '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
                           '-find', 'VC/Tools/MSVC/**/bin/Hostx64/x64/lib.exe'],
                          universal_newlines=True)
    candidates = [l.strip() for l in out.splitlines() if l.strip()]
    if not candidates:
        sys.exit('ERROR: lib.exe (MSVC x64 tools) not found by vswhere')
    # Pick the highest toolset version
    def version_key(path):
        m = re.search(r'MSVC[\\/]([0-9.]+)[\\/]', path)
        return [int(x) for x in m.group(1).split('.')] if m else []
    return max(candidates, key=version_key)

curdir = os.getcwd()

sys.path.append(os.path.join(curdir, '.ci'))
import cue
cue.detect_context()

cachedir = cue.ci['cachedir']
# With Make/perl, it's safer to use forward slashes (they don't disappear)
if cue.ci['os'] == 'windows':
    cachedir = cachedir.replace('\\', '/')

libxml2 = 'libxml2-2.9.3'
iconv = 'iconv-1.14'

dirs = {}
dirs[libxml2] = cachedir + '/' + libxml2
dirs[iconv] = cachedir + '/' + iconv

if 'OPEN62541' in os.environ:
    with open(os.path.join(curdir, 'configure', 'CONFIG_SITE.local'), 'a') as f:
        f.write('''
LIBXML2 = {0}
ICONV = {1}
'''.format(dirs[libxml2], dirs[iconv]))

    print('Wrote {0} as'.format(os.path.join(curdir, 'configure', 'CONFIG_SITE.local')))
    with open(os.path.join(curdir, 'configure', 'CONFIG_SITE.local'), 'r') as f:
        print(f.read())

    files = [ libxml2, iconv ]

    print('Downloading Windows MSVC dependencies ... libxml2 and iconv')
    sys.stdout.flush()
    for f in files:
      tar = f + '-win32-x86_64.7z'
      dir = dirs[f]

      try:
          os.makedirs(dir)
      except:
          pass

      sp.check_call(['curl', '-fsSL', '--retry', '3', '-O',
                     'http://xmlsoft.org/sources/win32/64bit/{0}'.format(tar)],
                    cwd=cue.toolsdir)
      sp.check_call(['7z', 'x', '-aoa', '-bd', os.path.join(cue.toolsdir, tar)], cwd=dir)

    lib = find_msvc_lib()
    print('Using gendef: {0}\nUsing lib.exe: {1}'.format(gendef, lib))
    sys.stdout.flush()

    libdir = os.path.join(dirs[libxml2], 'lib')
    sp.check_call([gendef, '../bin/libxml2-2.dll'], cwd=libdir)
    sp.check_call([lib, '/def:libxml2-2.def', '/machine:x64', '/out:libxml2.lib'], cwd=libdir)

    libdir = os.path.join(dirs[iconv], 'lib')
    sp.check_call([gendef, '../bin/libiconv-2.dll'], cwd=libdir)
    sp.check_call([lib, '/def:libiconv-2.def', '/machine:x64', '/out:libiconv.lib'], cwd=libdir)
