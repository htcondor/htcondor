#Chirp Java Client Makefile
#
#This public domain software is provided "as is".  See the Chirp License
#for details.

JFILES = \
ChirpConfig.java \
ChirpClient.java \
ChirpInputStream.java \
ChirpOutputStream.java

Chirp.jar: $(JFILES)
	javac $(JFILES)
	mkdir -p edu/wisc/cs/condor/chirp
	mv *.class edu/wisc/cs/condor/chirp
	jar cf Chirp.jar edu
	rm -rf edu

clean:
	rm -f Chirp.jar


