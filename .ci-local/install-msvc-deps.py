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
