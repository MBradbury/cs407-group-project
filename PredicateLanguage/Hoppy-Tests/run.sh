#!/bin/bash

THISDIR=$(basename "$(pwd)")
HOPPYDIR="../Hoppy"

if [[ $(uname) = "Linux" ]]
then
	CPSEP=":"
else
	CPSEP=";"
fi

CP="${HOPPYDIR}"

# Remove any previous intermediate files
rm -rf Intermediate Output
mkdir Intermediate Output

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
		echo "==================================Test $name Failed"
	else
		echo "==================================Test $name Succeeded"
	fi


	#name=$(basename $test)
	
	#echo "$VMBINARY Intermediate/$name > \"Output/$name\""

	#$VMBINARY Intermediate/$name > "Output/$name"
	
	# Check that we got the expected result
	
	#`cmp -s Output/$name Expected/$name >/dev/null`
	
	#if [[ $? != 0 ]]
	#then
	#	echo "==================================Test $name Failed"
	#else
	#	echo "==================================Test $name Succeeded"
	#fi
done
