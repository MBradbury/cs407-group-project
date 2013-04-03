#!/bin/bash
#
# This script will recursively iterate though the subdirectories
# of the directory it is executed in looking for graph.p files.
# When it finds them it will use gnuplot to create a graph from them.
#
# Author: Matthew Bradbury

# Detect if we have gnuplot-nox available
# if we do use that to create the graphs,
# otherwise just use plain old gnuplot.
result=`command -v gnuplot-nox`

if [[ "/usr/bin/gnuplot-nox" == $result ]]; then
	GNUPLOT="gnuplot-nox"
else
	GNUPLOT="gnuplot"
fi

# Enable for loops over items with spaces in their name
# From: http://heath.hrsoftworks.net/archives/000198.html
IFS=$'\n'

function create {

	for dir in `ls "$1"`
	do
		# Check if this is a directory
		if [ -d "$1/$dir" ]; then
		
			# Continue recursing
			create "$1/$dir"
		
		# Check if we have found a graph file
		elif [ "$dir" = "graph.p" ]; then
		
			# Remember the current working directory
			cwd=$PWD
			
			echo "Graphing $1"
		
			# Enter the directory holding the graph.p file
			# So the graph is created in the same directory
			cd "$1"
		
			# Use gnuplot-nox to prevent a window being created
			# every time we create a graph.
			$GNUPLOT ./graph.p
			
			# Crop the pdf to remove extra whitespace
			pdfcrop graph.pdf graph.pdf
		
			# Return to the previous current working directory
			cd "$cwd"
		fi
	done
}

# Start at the current directory
create "."

