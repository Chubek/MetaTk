#!/usr/bin/env python3
import os
import sys

os.write(3, ('\\textbf{' + sys.argv[1] + '}').encode('utf-8'))
