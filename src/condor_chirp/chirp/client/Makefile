#Chirp C Client Makefile
#
#This public domain software is provided "as is".  See the Chirp License
#for details.

OFILES = chirp_client.o

CPPFLAGS = -I ../common
CFLAGS = -fPIC

ifdef DEBUG_CHIRP
	CFLAGS += -D DEBUG_CHIRP
endif

libchirp_client.a: $(OFILES)
	$(AR) -rcs libchirp_client.a $(OFILES)

clean:
	rm -f *.o libchirp_client.a

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
