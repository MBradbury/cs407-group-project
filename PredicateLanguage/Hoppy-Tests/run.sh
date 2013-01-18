#!/bin/bash

THISDIR=$(basename "$(pwd)")
HOPPYDIR="../Hoppy"
DRAGONDIR="../Dragon"

if [[ $(uname) = "Linux" ]]
then
	CPSEP=":"
else
	CPSEP=";"
fi

CP="${HOPPYDIR}"
DRAGONCP="${DRAGONDIR}${CPSEP}${DRAGONDIR}/guava-13.0.1.jar"

# Remove any previous intermediate files
rm -rf Intermediate Output Dragon-Output
mkdir Intermediate Output Dragon-Output

rm -f Input/*~

# Make the Dragon assembler
cd $HOPPYDIR
make
cd ../$THISDIR



if [[ $(uname) = "Linux" ]]
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

	cat $test | java -cp "$CP" Hoppy > "Intermediate/$name"


	`cmp -s Intermediate/$name Intermediate-Expected/$name >/dev/null`
	
	if [[ $? != 0 ]]
	then
		echo "==================================Test $name Failed (Intermediate)"
	else
		echo "==================================Test $name Succeeded (Intermediate)"
	fi

	cat "Intermediate/$name" | java -cp "$DRAGONCP" Dragon > "Dragon-Output/$name"
	

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

