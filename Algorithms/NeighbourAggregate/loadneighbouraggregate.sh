#!/bin/bash
make clean
make
for i in 1 2 3 4 5 6 7 8 9 10
do
	z=$(($i-1))
	powerlevel=1
	rm -f neighbour-aggregate.sky #remove the old build object if it exists
	
	echo "Compiling for Node: $z"
	
	make TARGET=sky neighbour-aggregate DEFINES=HELLOWORLD,NODE_ID=$i,HELLO,POWER_LEVEL=$powerlevel MOTES=/dev/ttyUSB$z 2> builderrorlog.txt #build
	
	echo "Uploading to Node: $z"

	sudo -E make TARGET=sky neighbour-aggregate.upload DEFINES=HELLOWORLD,NODE_ID=$i,HELLO,POWER_LEVEL=$powerlevel MOTES=/dev/ttyUSB$z #have to upload separetly from building (due to sudo screwing things up)
	
	echo "Launching terminal"

	sudo -b gnome-terminal --title="NODE: $i" -x bash -c "cd $CS407DIR/Algorithms/HSend/; sudo -E make login MOTES=/dev/ttyUSB$z" &  #launch a terminal to connect to the ndoe
	
	echo ""
done