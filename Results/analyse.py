#!/bin/python

import os
import sys
import json
from pprint import pprint
import xml.etree.ElementTree as ET
import gzip
from numpy import mean, std


# The first thing we need to do is parse the simulation file
# to work how what nodes are neighbours of other nodes
def calculateNeighbours(simulation):
	tree = ET.parse(simulation)
	simNode = tree.getroot().find("simulation")

	txrange = float(simNode.find("radiomedium").find("transmitting_range").text)
	txrange2 = txrange * txrange

	nodeCoords = {}

	for childNode in simNode.findall("mote"):
		nodeId = None
		x = None
		y = None

		for confNode in childNode.findall("interface_config"):
			idNode = confNode.find("id")
			xNode = confNode.find("x")
			yNode = confNode.find("y")

			if idNode is not None:
				nodeId = int(idNode.text)

			if xNode is not None:
				x = float(xNode.text)

			if yNode is not None:
				y = float(yNode.text)

		nodeCoords[nodeId] = (x, y)

	#pprint(nodeCoords)

	def nodeDistance2(a, b):
		return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2

	n = {}

	for a in nodeCoords.keys():
		n[a] = []

		for b in nodeCoords.keys():
			if a != b:
				# This maths was taken from csc-compute-neighbor-stats
				if nodeDistance2(nodeCoords[a], nodeCoords[b]) <= txrange2:
					n[a].append(b)

	return n

#pprint(neighbours)
# We have now finished finding out neighbours


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
	def __init__(self, path, neighbours):
		with gzip.open(path, 'rb') as f:
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
		self.responsesReachedSink = 0
		self.totalPredicatesSent = 0

		self.totalPredicates = len(self.data[u"predicate"])

		self.predicatesFailed = 0
		self.predicatesSucceeded = 0

		self.predicatesCorrectlyEvaluated = 0
		self.predicatesIncorrectlyEvaluated = 0
		
		for pred in self.data[u"predicate"]:
			node = int(str(pred[u"node"]).split(".")[0])

			if node != int(pred[u"on"]):
				self.responsesReachedSink += 1

			if pred[u"result"] == 0:
				self.totalPredicatesSent += 1
				self.predicatesFailed += 1
			else:
				self.predicatesSucceeded += 1

			# Lets now evaluate the predicate ourselves
			r = predicate(node, neighbours[node], self.dataAt(pred[u"clock"]))

			if (r == (pred[u"result"] == 1)):
				self.predicatesCorrectlyEvaluated += 1
			else:
				self.predicatesIncorrectlyEvaluated += 1


		self.responsesReachedSinkPC = float(self.responsesReachedSink) / float(self.totalPredicatesSent)
		self.successRate = float(self.predicatesSucceeded) / float(self.totalPredicates)
		self.failureRate = float(self.predicatesFailed) / float(self.totalPredicates)

		self.pcCorrectlyEvaluted = float(self.predicatesCorrectlyEvaluated) / float(self.totalPredicates)


	# Gets the slot value of a given node at the given time
	def dataAt(self, time):
		return {
			nodeId: nodeData[u"slot"]
			for (nodeId, nodeData)
			in latestValues(self.data[u"stats"][u"TDMA"], u"STDMA", time).items()
		}



def meanStdAttr(items, attrName):
	return (mean([getattr(x, attrName) for x in items]), std([getattr(x, attrName) for x in items]))

def meanStdAttrXx(items, attrName, X):
	return (mean([getattr(x, attrName)[X] for x in items]), std([getattr(x, attrName)[X] for x in items]))

def meanStdAttrTxRx(items, attrName):
	return {"rx": meanStdAttrXx(items, attrName, u"rx"), "tx": meanStdAttrXx(items, attrName, u"tx")}

results = {}

for peType in os.listdir('TDMA'):
	results[peType] = {}

	for predicateDist in os.listdir('TDMA/' + peType):
		results[peType][predicateDist] = {}

		for size in os.listdir('TDMA/' + peType + "/" + predicateDist):
			results[peType][predicateDist][size] = {}

			neighbours = calculateNeighbours(
				'TDMA/' + peType + "/" + predicateDist + "/" + size + "/TDMA.csc")

			for period in os.listdir('TDMA/' + peType + "/" + predicateDist + "/" + size):

				if not os.path.isdir('TDMA/' + peType + "/" + predicateDist + "/" + size + "/" + period):
					continue

				results[peType][predicateDist][size][period] = {}

				path = 'TDMA/' + peType + "/" + predicateDist + "/" + size + "/" + period

				localResults = []

				for resultsFile in os.listdir(path):

					print(path + "/" + resultsFile)

					try:

						a = AnalyseFile(path + "/" + resultsFile, neighbours)

						localResults.append(a)

					except Exception as e:
						print(e)

				# We need to find the average and standard deviation

				results[peType][predicateDist][size][period]["pcResponsesReachedSink"] = meanStdAttr(localResults, "responsesReachedSinkPC")
				#results[peType][size]["pcSuccessRate"] = meanStdAttr(localResults, "successRate")
				results[peType][predicateDist][size][period]["pcCorrectlyEvaluted"] = meanStdAttr(localResults, "pcCorrectlyEvaluted")

				results[peType][predicateDist][size][period]["messagesPE"] = meanStdAttrTxRx(localResults, "peTotal")
				results[peType][predicateDist][size][period]["messagesTDMA"] = meanStdAttrTxRx(localResults, "TDMATotal")
				results[peType][predicateDist][size][period]["messagesTotal"] = meanStdAttrTxRx(localResults, "rimeTotal")


pprint(results)
