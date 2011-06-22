.PHONY: clean

CFLAGS =  -Wall -g -O0 -I./protocol

CLIENT_SRC     = client.c 
SERVER_SRC     = server.c server_sm_lib.c

SRC = $(CLIENT_SRC) $(SERVER_SRC)

CLIENT_OBJ     = $(CLIENT_SRC:.c=.o)
SERVER_OBJ     = $(SERVER_SRC:.c=.o)

PROTOCOL_LIB = protocol/cftp_protocol.a 

OBJ = $(CLIENT_OBJ) $(SERVER_OBJ) $(SHA1_OBJ)

all: client server

$(PROTOCOL_LIB):
	cd protocol; make

client: $(CLIENT_OBJ) $(PROTOCOL_LIB)
server: $(SERVER_OBJ) $(PROTOCOL_LIB)

clean:
	rm -rf $(OBJ) client server *~
	cd protocol; make clean
