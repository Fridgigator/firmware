#!/usr/bin/env python3

import sys
import subprocess	

for a in sys.argv[2:]:
	result = subprocess.run(["xtensa-esp32-elf-addr2line", "-pfiaC", "-e", sys.argv[1] ,a])
	print(result.stdout)

