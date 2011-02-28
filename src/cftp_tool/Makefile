.PHONY: clean

CFLAGS =  -Wall -g -O0

CLIENT_SRC = client.c client_lib.c
SERVER_SRC = server.c server_sm_lib.c server_sm/error.c server_sm/discovery.c server_sm/negotiation.c server_sm/transfer.c server_sm/teardown.c 
COMMON_SRC = utilities.c
SRC = $(CLIENT_SRC) $(SERVER_SRC) $(COMMON_SRC)

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)
COMMON_OBJ = $(COMMON_SRC:.c=.o)

SHA1_OBJ   = sha1.o

OBJ = $(CLIENT_OBJ) $(SERVER_OBJ) $(COMMON_OBJ) $(SHA1_OBJ)

all: client server

$(SHA1_OBJ):
	cd sha1-c; make

client: $(CLIENT_OBJ) $(COMMON_OBJ) $(SHA1_OBJ)
server: $(SERVER_OBJ) $(COMMON_OBJ) $(SHA1_OBJ)

clean:
	rm -rf $(OBJ) client server *~
	cd sha1-c; make clean
