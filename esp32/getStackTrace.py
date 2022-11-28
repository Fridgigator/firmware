#!/usr/bin/env python3

import sys
import subprocess	

for a in sys.argv[1:]:
	result = subprocess.run(["xtensa-esp32-elf-addr2line", "-pfiaC", "-e", "./.pio/build/esp32dev/firmware.elf", a])
	print(result.stdout)

