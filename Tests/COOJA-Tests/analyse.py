#!/bin/python

import json
from pprint import pprint

def latestValues(values, keyName):
	result = {}

	for value in values:

		nodeId = value[keyName]

		# Not seen this node before
		if nodeId not in result:
			result[nodeId] = value
			del result[nodeId][keyName]

		# Seen this node before
		else:
			stored = result[nodeId]

			# Check if this is a latter time
			if value[u"clock"] > stored[u"clock"]:
				# Newer so update
				result[nodeId] = value
				del result[nodeId][keyName]

	return result

def totalSentRecv(moteResults):
	rx = 0
	tx = 0
	for value in moteResults.values():
		rx += value[u"rx"]
		tx += value[u"tx"]

	return {u"rx": rx, u"tx": tx}

class AnalyseFile:
	def __init__(self, path):
		with open(path) as f:
			self.data = json.load(f)

		self.motes = self.data[u"motes"]

		# Extract the last energy results
		self.rime = latestValues(self.data[u"stats"][u"rime"], u"S")
		self.energy = latestValues(self.data[u"stats"][u"energy"], u"E")
		self.TDMA = latestValues(self.data[u"stats"][u"TDMA"], u"STDMA")

		# Calculate how much energy predicate evaulation required
		self.pe = {}
		for mote in self.motes:
			result = {}

			total = self.rime[mote]
			tdma = self.TDMA[mote]

			for key in (u"tx", u"rx"):
				result[key] = total[key] - tdma[key]

			self.pe[mote] = result



a = AnalyseFile('COOJA.testlog')

#pprint(a.data)

#pprint(a.energy)
print("Total Messages: {0}".format(totalSentRecv(a.rime)))
print("TDMA: {0}".format(totalSentRecv(a.TDMA)))
print("PE: {0}".format(totalSentRecv(a.pe)))
