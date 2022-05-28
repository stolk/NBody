#!/usr/bin/python3

import sys
import os


def get_policies(dname) :
	l = os.listdir(dname)
	return [ dname+x for x in l if "policy" in x ]

def get_governors(dname) :
	f = open(dname + "policy0/scaling_available_governors")
	line = f.readline()
	f.close()
	return line.strip().split(" ")


def __main__() :
	dname = "/sys/devices/system/cpu/cpufreq/"
	policies = get_policies(dname)
	governors = get_governors(dname)

	if len(sys.argv) != 2 :
		print("Usage:", sys.argv[0], '|'.join(governors))
		sys.exit(1)

	gname = sys.argv[1]
	if not gname in governors:
		print(gname, "is not a valid governor for this system:", '|'.join(governors))
		sys.exit(2)

	if not os.access(dname, os.W_OK):
		print("No write permission. Use sudo to run as root.")
		sys.exit(3)

	for policy in policies:
		cmd = "echo " + gname + " > " + policy + "/scaling_governor"
		os.system(cmd)

__main__()

