#!/bin/bash
#make clean
make
for i in 1 2 3 4 5 6 7 8 9 10
do
	z=$(($i-1))
	rm -f hsend.sky #remove the old build object if it exists
	
	echo "Compiling for Node: $z"
	
	make TARGET=sky hsend DEFINES=HELLOWORLD,NODE_ID=$i,HELLO,POWER_LEVEL=1 MOTES=/dev/ttyUSB$z 2> builderrorlog.txt #build
	
	echo "Uploading to Node: $z"

	sudo make TARGET=sky hsend.upload DEFINES=HELLOWORLD,NODE_ID=$i,HELLO,POWER_LEVEL=1 MOTES=/dev/ttyUSB$z #have to upload separetly from building (due to sudo screwing things up)
	
	echo "Launching terminal"

	sudo -b gnome-terminal --title="NODE: $z" -x bash -c "cd /media/sf_cs407/Algorithms/HSend/; sudo -E make login MOTES=/dev/ttyUSB$z" &  #launch a terminal to connect to the ndoe
	
	echo ""
done