
JAVAC = javac
JAVACFLAGS = -g

JAR = jar
JARFLAGS =

CC = gcc
CCFLAGS = -Wall -g

LD = gcc
LDFLAGS =

AR = ar
ARFLAGS =

all: Chirp.jar chirp_test

Chirp.jar: ChirpTest.java ChirpClient.java ChirpConfig.java ChirpInputStream.java ChirpOutputStream.java
	${JAVAC} ${JAVACFLAGS} -d . $^
	${JAR} ${JARFLAGS} cf $@ edu

chirp_test: chirp_test.o chirp_client.o
	${LD} ${LDFLAGS} $^ -o $@ -lnsl

chirp_test.o: chirp_test.c chirp_client.h
	${CC} ${CCFLAGS} $< -c -o $@

chirp_client.o: chirp_client.c chirp_client.h chirp_protocol.h
	${CC} ${CCFLAGS} $< -c -o $@

clean:
	rm -rf edu *.class *.jar *.a *.o chirp_test
