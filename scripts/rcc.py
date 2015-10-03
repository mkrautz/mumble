#!/usr/bin/env python
#
# Copyright (C) 2015 Mikkel Krautz <mikkel@krautz.dk>
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# - Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# - Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# - Neither the name of the Mumble Developers nor the names of its
#   contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from __future__ import (unicode_literals, print_function, division)

import os
import platform
import sys
import subprocess
from xml.dom import minidom

def listmode()

def main():
	# Our rcc should list all files regardless of whether
	# they exist or not.
	listMode = '-list' in sys.argv or '--list' in sys.argv
	if listMode:
		inputs = sys.argv[5:] # strip out python rcc.py path/to/rcc.exe -list -name XXX
		for fn in inputs:
			with open(fn, 'r') as f:
				absFn = os.path.abspath(fn)
				fnDir = os.path.dirname(absFn)
				s = f.read()
				dom = minidom.parseString(s)
				fileTags = dom.getElementsByTagName('file')
				for fileTag in fileTags:
					textNode = fileTag.childNodes[0].wholeText
					absPath = os.path.normpath(os.path.join(fnDir, textNode))
					relPath = os.path.relpath(absPath)
					output = relPath
					if platform.system() == 'Windows':
						output = output.replace('\\', '/')
					print(output)
		sys.exit(0)

	ret = subprocess.call(sys.argv[1:], shell=True)
	sys.exit(ret)

if __name__ == '__main__':
	main()
