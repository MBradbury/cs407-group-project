all:
	javacc Hoppy.jj
	javac *.java

jar: all
	rm -rf predvis
	mkdir predvis
	chmod -R 744 predvis
	mkdir predvis/hoppy
	chmod -R 744 predvis/hoppy
	cp *.class predvis/hoppy
	chmod 644 predvis/hoppy/*.class
	jar cvf Hoppy.jar predvis

clean:
	rm -rf *.java *.class Hoppy.jar predvis
