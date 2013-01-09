UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
	CPSEP=":"
else
	CPSEP=";"
endif

all:
	javacc Dragon.jj
	javac -cp .$(CPSEP)guava-13.0.1.jar *.java

clean:
	rm *.java *.class
