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
	mkdir predvis
	mkdir predvis/dragon
	cp *.class predvis/dragon
	jar cvf Dragon.jar predvis

clean:
	rm -rf *.java *.class Dragon.jar predvis
