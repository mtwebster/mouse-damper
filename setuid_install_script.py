#!/usr/bin/python3

import os
import sys
import subprocess

if not os.environ.get('DESTDIR'):
    print('Changing mouse-damper owner to root and setting setuid flag so it can run as root for a normal user...')
    subprocess.run(['chown', 'root:root', sys.argv[1]])
    subprocess.run(['chmod', 'u+s', sys.argv[1]])
