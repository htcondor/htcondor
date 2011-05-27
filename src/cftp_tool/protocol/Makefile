.PHONY: clean

CFLAGS =  -Wall -g -O0

COMMON_SCR     = common.c\
		 utilities.c

SEND_SRC       = send.c\
		 send/error.c\
		 send/discovery.c\
		 send/negotiation.c\
		 send/transfer.c\
		 send/teardown.c 

RECEIVE_SRC    = receive.c\
		 receive/error.c\
		 receive/discovery.c\
		 receive/negotiation.c\
		 receive/transfer.c\
		 receive/teardown.c 

SRC = $(RECEIVE_SRC) $(COMMON_SRC)


SHA1_OBJ   = sha1.o


OBJ = $(SRC:.c=.o) $(SHA1_OBJ)

all: cftp_protocol.a

$(SHA1_OBJ):
	cd sha1-c; make

cftp_protocol.a: $(OBJ)
	ar -rcs $@ $?

clean:
	rm -rf $(OBJ) cftp_protocol.a *~
	rm -rf recieve/*~
	rm -rf send/*~
	rm -rf sha1-c/*~
	cd sha1-c; make clean
