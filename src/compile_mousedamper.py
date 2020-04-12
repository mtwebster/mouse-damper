#!/usr/bin/env python3
import os
import sys
print(os.getcwd())
os.system("nuitka3 --standalone %s" % sys.argv[1])
os.system("mv mousedamper.dist src/mouse-damper")
