all:
	javacc Hoppy.jj
	javac *.java

jar: all
	jar cvf Hoppy.jar *.class

clean:
	rm *.java *.class Hoppy.jar
