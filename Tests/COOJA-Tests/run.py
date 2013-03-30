#!/bin/python

import time
import subprocess
import multiprocessing

# Running
coojaDir = "/home/user/cs407-group-project/contiki/tools/cooja"
cscPath = "/home/user/TDMA.csc"
command = ["ant", "run_nogui", "-Dargs={0}".format(cscPath)]

# Simulation
repeats = 12
simultaneousProcs = multiprocessing.cpu_count()


processes = []
removedCount = 0

# Keep adding more processes if we still need to do more repeats
while removedCount < repeats:

	# Check to see if any processes have finished
	removedCount += len([p for p in processes if p.poll() is not None])
	processes = [p for p in processes if p.poll() is None]

	# Create processes if below limit
	if len(processes) < simultaneousProcs and (removedCount + len(processes)) < repeats:

		print("Adding new process {0}/{1}".format(removedCount + len(processes) + 1, repeats))

		p = subprocess.Popen(command, cwd=coojaDir, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

		processes.append(p)

	# Wait for a bit
	time.sleep(1)
