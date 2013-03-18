UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
	CPSEP=":"
else ifeq ($(UNAME), Darwin)
	CPSEP=":"
else
	CPSEP=";"
endif

all:
	javacc Dragon.jj
	javac -cp .$(CPSEP)guava-13.0.1.jar *.java

jar: all
	jar cvf Dragon.jar *.class

clean:
	rm *.java *.class Dragon.jar
