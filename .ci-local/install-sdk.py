#!/usr/bin/env python
"""CI script to install a binary evaluation version of the UA SDK
"""

from __future__ import print_function

import sys, os
import shutil
import subprocess as sp
import re

curdir = os.getcwd()

sys.path.append(os.path.join(curdir, '.ci'))
import cue
cue.detect_context()

sourcedir = os.path.join(cue.homedir, '.source')
sdkdir = os.path.join(sourcedir, 'sdk')

# With Make/perl, it's safer to use forward slashes (they don't disappear)
if cue.ci['os'] == 'windows':
    sourcedir = sourcedir.replace('\\', '/')
    sdkdir = sdkdir.replace('\\', '/')

if 'UASDK' in os.environ:
    with open(os.path.join(curdir, 'configure', 'CONFIG_SITE.local'), 'a') as f:
        f.write('''
UASDK = {0}
UASDK_DEPLOY_MODE = PROVIDED
UASDK_LIB_DIR = $(UASDK)/lib
UASDK_SHRLIB_DIR = $(UASDK)/bin
UASDK_USE_CRYPTO = YES
UASDK_USE_XMLPARSER = YES'''.format(sdkdir))
        if cue.ci['os'] == 'windows':
            f.write('''
LIBXML2 = $(UASDK)/third-party/win64/vs2015/libxml2
OPENSSL = $(UASDK)/third-party/win64/vs2015/openssl''')

    if os.environ['UASDK'].startswith('1.5.'):
        uasdkcc = 'gcc4.7.2'
    else:
        uasdkcc = 'gcc6.3.0'

    try:
        os.makedirs(sourcedir)
        os.makedirs(cue.toolsdir)
    except:
        pass

    if cue.ci['os'] == 'windows':
        tar_name = 'uasdk-win64-vs2015-{0}.zip'.format(os.environ['UASDK'])
    else:
        tar_name = 'uasdk-x86_64-{0}-{1}.tar.gz'.format(uasdkcc, os.environ['UASDK'])
    enc_name = tar_name + '.enc'

    print('Downloading UA SDK {0}'.format(os.environ['UASDK']))
    sys.stdout.flush()
    sp.check_call(['curl', '-fsSL', '--retry', '3', '-o', enc_name,
                   'https://github.com/ralphlange/opcua-client-libs/raw/master/{0}'
                  .format(enc_name)],
                  cwd=cue.toolsdir)
    sp.check_call(['openssl', 'aes-256-cbc', '-md', 'md5',
                   '-k', os.environ['encrypted_178ee45b7f75_pass'],
                   '-in', enc_name, '-out', tar_name, '-d'], cwd=cue.toolsdir)

    if os.path.exists(sdkdir):
        shutil.rmtree(sdkdir)

    if cue.ci['os'] == 'windows':
        sp.check_call(['7z', 'x', '-aoa', '-bd', os.path.join(cue.toolsdir, tar_name)], cwd=sourcedir)
    else:
        sp.check_call(['tar', '-C', sourcedir, '-xz', '-f', os.path.join(cue.toolsdir, tar_name)])

    os.remove(os.path.join(cue.toolsdir, enc_name))
    os.remove(os.path.join(cue.toolsdir, tar_name))
