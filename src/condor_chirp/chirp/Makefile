#Chirp Library Makefile
#
#This public domain software is provided "as is".  See the Chirp License
#for details.

.PHONY: all client java-client release clean dist-clean

client:
	$(MAKE) -C client
	mkdir -p lib
	cp client/libchirp_client.a lib

	mkdir -p include
	cp client/chirp_client.h include

java-client:
	$(MAKE) -C java/client

clean:
	$(MAKE) -C client clean
	$(MAKE) -C java/client clean

dist-clean: clean
	rm -f include/chirp_client.h
	rm -f lib/libchirp_client.a
	if [ -d include ]; then rmdir include ; fi
	if [ -d lib ]; then rmdir lib ; fi

