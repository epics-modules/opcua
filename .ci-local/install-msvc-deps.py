#!/usr/bin/env python
"""CI script to install Windows dependencies (libxml2 and libiconv)
"""

from __future__ import print_function

import sys, os
import shutil
import fileinput
import subprocess as sp
import re

curdir = os.getcwd()

sys.path.append(os.path.join(curdir, '.ci'))
import cue
cue.detect_context()

cachedir = cue.ci['cachedir']
# Special case: MSYS2 shell
if cue.ci['os'] == 'windows' and os.sep == '/':
    cachedir = cachedir.replace('\\', '/')

if 'OPEN62541' in os.environ:
    with open(os.path.join(curdir, 'configure', 'CONFIG_SITE.local'), 'a') as f:
        f.write('''
LIBXML2 = {0}
ICONV = {1}
'''.format(os.path.join(cachedir, 'libxml2-2.9.3'), os.path.join(cachedir, 'iconv-1.14')))

    print('Wrote {0} as'.format(os.path.join(curdir, 'configure', 'CONFIG_SITE.local')))
    with open(os.path.join(curdir, 'configure', 'CONFIG_SITE.local'), 'r') as f:
        print(f.read())

    files = [ 'libxml2-2.9.3-win32-x86_64.7z', 'iconv-1.14-win32-x86_64.7z' ]
    print('Downloading Windows dependencies ...')
    sys.stdout.flush()
    for f in files:
      sp.check_call(['curl', '-fsSL', '--retry', '3', '-O',
                     'http://xmlsoft.org/sources/win32/64bit/{0}'.format(f)],
                    cwd=cue.toolsdir)
      sp.check_call(['7z', 'x', '-aoa', '-bd', os.path.join(cue.toolsdir, f)], cwd=cachedir)


      sp.check_call(['dir'], cwd=cachedir)
