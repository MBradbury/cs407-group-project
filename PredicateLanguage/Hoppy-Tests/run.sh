#!/bin/bash

THISDIR=$(basename "$(pwd)")
HOPPYDIR="../Hoppy"
DRAGONDIR="../Dragon"

if [[ $(uname) = "Linux" || $(uname) = "Darwin" ]]
then
	CPSEP=":"
else
	CPSEP=";"
fi

CP="${HOPPYDIR}/Hoppy.jar"
DRAGONCP="${DRAGONDIR}/Dragon.jar${CPSEP}${DRAGONDIR}/guava-13.0.1.jar"

# Remove any previous intermediate files
rm -rf Intermediate Output Dragon-Output
mkdir Intermediate Output Dragon-Output

rm -f Input/*~

# Make Hoppy compiler
cd $HOPPYDIR
make jar
cd ../$THISDIR


# Make the Dragon assembler
cd $DRAGONDIR
make jar
cd ../$THISDIR



if [[ $(uname) = "Linux" || $(uname) = "Darwin" ]]
then
	cd ../
	make
	cd $THISDIR
	
	VMBINARY="../predlang"
else
	VMBINARY="../Debug/PredicateLanguage.exe"
fi

for test in Input/*
do
	name=$(basename $test)

	cat $test | java -cp "$CP" predvis.hoppy.Hoppy > "Intermediate/$name"


	`cmp -s Intermediate/$name Intermediate-Expected/$name >/dev/null`
	
	if [[ $? != 0 ]]
	then
		echo "==================================Test $name Failed (Intermediate)"
	else
		echo "==================================Test $name Succeeded (Intermediate)"
	fi

	cat "Intermediate/$name" | java -cp "$DRAGONCP" predvis.dragon.Dragon > "Dragon-Output/$name"
	

	#echo "$VMBINARY \"Dragon-Output/$name\" > \"Output/$name\""

	$VMBINARY "Dragon-Output/$name" > "Output/$name"
	
	# Check that we got the expected result
	
	`cmp -s Output/$name Expected/$name >/dev/null`
	
	if [[ $? != 0 ]]
	then
		echo "==================================Test $name Failed"
	else
		echo "==================================Test $name Succeeded"
	fi
done

