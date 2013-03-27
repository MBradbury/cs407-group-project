all:
	javacc Hoppy.jj
	javac *.java

jar: all
	rm -rf predvis
	mkdir predvis
	mkdir predvis/hoppy
	cp *.class predvis/hoppy
	jar cvf Hoppy.jar predvis

clean:
	rm -rf *.java *.class Hoppy.jar predvis
