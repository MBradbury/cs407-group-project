#!/bin/python

import sys

input_file = sys.argv[1]

bytes = []

with open(input_file, 'rb') as f:
	byte = f.read(1)

	if len(byte) != 0:
		bytes.append('0x{0:02X}'.format(ord(byte)))

	while byte:
		byte = f.read(1)

		if len(byte) != 0:
			bytes.append('0x{0:02X}'.format(ord(byte)))

print(bytes)

print(','.join(bytes))
