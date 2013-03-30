#!/bin/python

import json
from pprint import pprint

def latestValues(values, keyName, maxTime=None):
	result = {}

	for value in values:

		nodeId = value[keyName]

		# Not seen this node before
		if nodeId not in result:
			result[nodeId] = value

		# Seen this node before
		else:
			stored = result[nodeId]

			# Check if this is a latter time
			if value[u"clock"] > stored[u"clock"]:
				if maxTime is None or value[u"clock"] <= maxTime:
					# Newer so update
					result[nodeId] = value

	return result

def totalSentRecv(moteResults):
	rx = 0
	tx = 0
	for value in moteResults.values():
		rx += value[u"rx"]
		tx += value[u"tx"]

	return {u"rx": rx, u"tx": tx}

# Evaluates the following predicate
# This can be used to get the expected result
#using Neighbours(1) as onehopn in
#    	@(a : onehopn ~
#    		slot(a) != slot(this) &
#    	    @(b : onehopn ~ addr(a) != addr(b) => slot(a) != slot(b))
#    	)
def predicate(this, onehopn, slots):
	result = True
	for a in onehopn:
		result &= slots[a] != slots[this]
		for b in onehopn:
			result &= (a == b or slots[a] != slots[b])
	return result

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

		# Calculate totals
		self.rimeTotal = totalSentRecv(self.rime)
		self.TDMATotal = totalSentRecv(self.TDMA)
		self.peTotal = totalSentRecv(self.pe)

		# Predicate evaluation analysis

	# Gets the slot value of a given node at the given time
	def dataAt(self, time):
		return {
			nodeId: nodeData[u"slot"]
			for (nodeId, nodeData)
			in latestValues(self.data[u"stats"][u"TDMA"], u"STDMA", time).items()
		}


a = AnalyseFile('COOJA.testlog')

pprint(a.data[u"predicate"])

#pprint(a.energy)
print("Total Messages: {0}".format(a.rimeTotal))
print("TDMA: {0}".format(a.TDMATotal))
print("PE: {0}".format(a.peTotal))

print(a.dataAt(462))
print(a.dataAt(900))
