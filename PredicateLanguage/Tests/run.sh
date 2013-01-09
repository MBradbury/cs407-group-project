#!/bin/bash

DRAGONDIR="../Dragon"

if [[ $(uname) = "Linux" ]]
then
	CPSEP=":"
else
	CPSEP=";"
fi

CP="${DRAGONDIR}${CPSEP}${DRAGONDIR}/guava-13.0.1.jar"

# Remove any previous intermediate files
rm -rf Intermediate Output
mkdir Intermediate Output

# Make the Dragon assembler
cd $DRAGONDIR
make
cd ../Tests

for test in Input/*
do
	echo $CP

	cat $test | java -cp "$CP" Dragon > "Intermediate/$(basename $test)"
done


if [[ $(uname) = "Linux" ]]
then
	cd ../
	make
	cd Tests
	
	VMBINARY="../predlang"
else
	VMBINARY="../Debug/PredicateLanguage.exe"
fi



for test in Intermediate/*
do
	$VMBINARY $test > "Output/$(basename $test)"
done

