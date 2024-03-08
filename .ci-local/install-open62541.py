#!/usr/bin/env python
"""CI script to install and compile the open62541 SDK
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

version = os.environ['OPEN62541']

sourcedir = os.path.join(cue.homedir, '.source')
sdkdir = os.path.join(sourcedir, 'open62541-{0}'.format(version))
builddir = os.path.join(sdkdir, 'build')
installdir = os.path.join(cue.ci['cachedir'], 'open62541')

# With Make/perl, it's safer to use forward slashes (they don't disappear)
if cue.ci['os'] == 'windows':
    sourcedir = sourcedir.replace('\\', '/')
    sdkdir = sdkdir.replace('\\', '/')
    builddir = builddir.replace('\\', '/')
    installdir = installdir.replace('\\', '/')

if 'OPEN62541' in os.environ:
    with open(os.path.join(curdir, 'configure', 'CONFIG_SITE.local'), 'a') as f:
        f.write('''
OPEN62541 = {0}
OPEN62541_DEPLOY_MODE = PROVIDED
OPEN62541_LIB_DIR = $(OPEN62541)/lib
OPEN62541_SHRLIB_DIR = $(OPEN62541)/bin
OPEN62541_USE_CRYPTO = YES
OPEN62541_USE_XMLPARSER = YES'''.format(installdir))

    try:
        os.makedirs(sourcedir)
        os.makedirs(cue.toolsdir)
    except:
        pass

    print('Downloading open62541 sources v{0}'.format(version))
    sys.stdout.flush()
    tar_name = 'open62541-{0}.tar.gz'.format(version)
    sp.check_call(['curl', '-fsSL', '--retry', '3', '-o', tar_name,
                   'https://github.com/open62541/open62541/archive/refs/tags/v{0}.tar.gz'
                  .format(version)],
                  cwd=cue.toolsdir)

    if os.path.exists(sdkdir):
        shutil.rmtree(sdkdir)
    sp.check_call(['tar', '-C', sourcedir, '-xz', '-f', os.path.join(cue.toolsdir, tar_name)])
    os.remove(os.path.join(cue.toolsdir, tar_name))

    try:
        os.makedirs(builddir)
    except:
        pass

    print('Compiling & installing open62541 v{0}'.format(version))

    ver = version.split('.')
    if ver[0] == '1' and ver[1] == '2':
        for line in fileinput.input(os.path.join(sdkdir, 'CMakeLists.txt'), inplace=True):
            print(line.replace('set_open62541_version()',
            'set(OPEN62541_VER_MAJOR 1)\n'
            'set(OPEN62541_VER_MINOR 2)\n'
            'set(OPEN62541_VER_PATCH 7)\n'
            'set(OPEN62541_VER_LABEL -undefined)'.format(ver[0], ver[1], ver[2])), end='')

    generator = 'Unix Makefiles'
    if cue.ci['os'] == 'windows':
        if cue.ci['compiler'] == 'gcc':
            generator = 'MinGW Makefiles'
        elif cue.ci['compiler'] == 'vs2019':
            generator = 'Visual Studio 16 2019'

    build_shared = 'ON'
    if cue.ci['static']:
        build_shared = 'OFF'

    sp.check_call(['cmake', '..',
                   '-G', generator,
                   '-DBUILD_SHARED_LIBS={0}'.format(build_shared),
                   '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
                   '-DUA_ENABLE_ENCRYPTION=OPENSSL',
                   '-DUA_ENABLE_ENCRYPTION_OPENSSL=ON',
                   '-DCMAKE_INSTALL_PREFIX={0}'.format(installdir)],
                   cwd=builddir)

    sp.check_call(['cmake', '--build', '.', '--config', 'RelWithDebInfo'], cwd=builddir)
    sp.check_call(['cmake', '--install', '.', '--config', 'RelWithDebInfo'], cwd=builddir)
