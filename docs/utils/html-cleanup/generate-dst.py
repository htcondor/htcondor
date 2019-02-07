#!/usr/bin/env python

import os
import sys

with open("out-files.txt") as out_file:
	file_list = out_file.readlines()
out_file.close()

out_data = ""

for filename in file_list:
	out_data += "pandoc -f html -t rst " + filename.rstrip() + " /../../../docs/rst/" + filename.rstrip() + ".rst\n"

sh_script = open("out2rst.sh", "w")
sh_script.write(out_data)
sh_script.close()

