.PHONY: clean

CFLAGS =  -Wall -g -O0 -I../protocol

CLIENT_SRC     = client.c 
SERVER_SRC     = server.c server_sm_lib.c

SRC = $(CLIENT_SRC) $(SERVER_SRC)

CLIENT_OBJ     = $(CLIENT_SRC:.c=.o)
SERVER_OBJ     = $(SERVER_SRC:.c=.o)

OBJ = $(CLIENT_OBJ) $(SERVER_OBJ) $(SHA1_OBJ)

all: client server

client: $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) ../lib/cftp_protocol.a -o client


server: $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) ../lib/cftp_protocol.a -o server

clean:
	rm -rf $(OBJ) client server *~