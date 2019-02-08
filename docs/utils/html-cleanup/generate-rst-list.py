#!/usr/bin/env python

import os
import sys

with open("port-manual.sh") as port_sh:
	port_data = port_sh.readlines()
port_sh.close()

rst_data = ""

for line in port_data:
	if "pandoc" in line:
		line_tokens = line.split("/")
		print("   " + str(line_tokens[5])[:-5].rstrip())

#sh_script = open("out2rst.sh", "w")
#sh_script.write(out_data)
#sh_script.close()

