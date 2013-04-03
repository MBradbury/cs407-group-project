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
#		@(a : onehopn ~
#			slot(a) != slot(this) &
#			@(b : onehopn ~ addr(a) != addr(b) => slot(a) != slot(b))
#		)
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

		self.pcCorrectlyEvaluated = float(self.predicatesCorrectlyEvaluated) / float(self.totalPredicates)


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
				results[peType][predicateDist][size][period]["pcCorrectlyEvaluated"] = meanStdAttr(localResults, "pcCorrectlyEvaluated")

				results[peType][predicateDist][size][period]["messagesPE"] = meanStdAttrTxRx(localResults, "peTotal")
				results[peType][predicateDist][size][period]["messagesTDMA"] = meanStdAttrTxRx(localResults, "TDMATotal")
				results[peType][predicateDist][size][period]["messagesTotal"] = meanStdAttrTxRx(localResults, "rimeTotal")


pprint(results)


# Done with the processing of results, now lets generate some graph files


# Some utility functions
# From:  http://ginstrom.com/scribbles/2007/09/04/pretty-printing-a-table-in-python/
def get_max_width(table, index):
	"""Get the maximum width of the given column index."""

	return max([len(str(row[index])) for row in table])
	
# From:  http://ginstrom.com/scribbles/2007/09/04/pretty-printing-a-table-in-python/
def pprint_table(out, table):
	"""Prints out a table of data, padded for alignment
	@param out: Output stream (file-like object)
	@param table: The table to print. A list of lists.
	Each row must have the same number of columns."""

	col_paddings = []

	for i in range(len(table[0])):
		col_paddings.append(get_max_width(table, i))

	for row in table:
		# left col
		out.write(str(row[0]).ljust(col_paddings[0] + 1))
		
		# rest of the cols
		for i in range(1, len(row)):
			out.write(str(row[i]).rjust(col_paddings[i] + 2))
		
		out.write('\n')

# From: http://stackoverflow.com/questions/273192/python-best-way-to-create-directory-if-it-doesnt-exist-for-file-write
def ensureDir(f):
	d = os.path.dirname(f)
	if not os.path.exists(d):
		os.makedirs(d)
		
def keyToDirName(algorithm, bytecode, about, accessor):
	return 'Graphs/' + algorithm + '/' + bytecode + '/' + about + '/' + ('' if accessor is None else accessor + '/')
		
def graph(allvalues, algorithm, title, labelX, labelY, keyName, accessorKey=None, rangeY=None, keypos='right top', kind='pdf'):

	rearranged = {}

	for (bytecode, items1) in allvalues[algorithm].items():
		for (size, items2) in items1.items():
			for (period, items3) in items2.items():
				dirName = keyToDirName(algorithm, bytecode, keyName, accessorKey)
				
				# Ensure that the dir we want to put the files in
				# actually exists
				ensureDir(dirName)
			
				if accessorKey is None:
					rearranged.setdefault((algorithm, bytecode, keyName, None), {}).setdefault(int(size), {})[period] = items3[keyName]
				else:
					rearranged.setdefault((algorithm, bytecode, keyName, accessorKey), {}).setdefault(int(size), {})[period] = items3[keyName][accessorKey]
				
	pprint(rearranged)
	
	for (key, values) in rearranged.items():
		dirName = keyToDirName(*key)
		
		sizes = list(sorted(values.keys()))
		
		varying = values[sizes[0]].keys()
		
		# Write our data
		datFileName = dirName + 'graph.dat'
		with open(datFileName, 'w') as datFile:

			table = [ ['#Size'] + ['Value', 'StdDev']*len(varying) ]
			
			# We want to print out rows in the correct
			# size order, so iterate through sizes this way
			for size in sizes:
				row = [ size ]
				for vary in varying:
					row += [ values[size][vary][0], values[size][vary][1] ]
					
				table.append( row )
			
			pprint_table(datFile, table)
	
		# Write out the graph definition file
		pFileName = dirName + 'graph.p'
		with open(pFileName, 'w') as pFile:

			pFile.write('set xlabel "{0}"\n'.format(labelX))
			pFile.write('set ylabel "{0}"\n'.format(labelY))
			pFile.write('set pointsize 1\n')
			pFile.write('set key {0}\n'.format(keypos))
			pFile.write('set title "{0}"\n'.format(title))

			# Should remain the same as we are testing with
			# a limited sized grid of nodes
			pFile.write('set xrange [{0}:{1}]\n'.format(min(sizes) - 1, max(sizes) + 1))
			pFile.write('set xtics ({0})\n'.format(','.join(map(str, sizes))))

			if rangeY is not None:
				pFile.write('set yrange [{0}:{1}]\n'.format(rangeY[0], rangeY[1]))
			else:
				pFile.write('set yrange auto\n')
				
			pFile.write('set ytics auto\n')
			
			if kind == 'pdf':
				pFile.write('set terminal pdf enhanced\n')
				pFile.write('set output "graph.pdf" \n')
			elif kind == 'ps':
				pFile.write('set terminal postscript enhanced 22\n')
				pFile.write('set output "graph.ps"\n')
			else:
				pFile.write('set terminal postscript eps enhanced 22\n')
				pFile.write('set output "graph.eps"\n')
			
			pFile.write('plot ')
			
			for (i, vary) in enumerate(varying):
			
				valueIndex = 2 * (i + 1)
				stddevIndex = valueIndex + 1
			
				pFile.write('"graph.dat" u 1:2:3 w errorlines ti "Period={0} sec"'.format(vary, valueIndex, stddevIndex))
				
				if i + 1 != len(varying):
					pFile.write(',\\\n')
			
			pFile.write('\n')

for predicate in results.keys():		
	graph(results, predicate, predicate + '(Correctly Evaluated)', 'Network Size', 'Percentage Correctly Evaluated', 'pcCorrectlyEvaluated', rangeY=(0, 1))
	
	# Makes no sense to graph global results reaching the sink
	if predicate in ('PELP', 'PELE'):
		graph(results, predicate, predicate + '(Response Reached Sink)', 'Network Size', 'Percentage Correctly Evaluated', 'pcResponsesReachedSink', rangeY=(0, 1))
	
	graph(results, predicate, predicate + '(PE Tx)', 'Network Size', 'Messages Sent', 'messagesPE', accessorKey='tx', rangeY=(0, '*'))
	
	graph(results, predicate, predicate + '(PE Rx)', 'Network Size', 'Messages Received', 'messagesPE', accessorKey='rx', rangeY=(0, '*'))

