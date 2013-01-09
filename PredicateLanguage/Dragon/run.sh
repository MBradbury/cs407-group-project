#!/bin/bash
make; echo -e "IVAR 1\nIPUSH 1\nISTORE 1\nIPOP\nIFETCH 1\nIPUSH 2\nIADD\nIPUSH 3\nIEQ\nHALT" | java -cp .:guava-13.0.1.jar Dragon > program; ../predlang program

