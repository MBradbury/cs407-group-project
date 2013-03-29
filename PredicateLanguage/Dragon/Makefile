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
	rm -rf predvis
	mkdir predvis
	chmod -R 744 predvis
	mkdir predvis/dragon
	chmod -R 744 predvis/dragon
	cp *.class predvis/dragon
	chmod 644 predvis/dragon/*.class
	jar cvf Dragon.jar predvis

clean:
	rm -rf *.java *.class Dragon.jar predvis
