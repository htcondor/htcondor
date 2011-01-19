CFLAGS =  -Wall

CLIENT_SRC = client.c client_lib.c
SERVER_SRC = server.c server_lib.c
COMMON_SRC = utilities.c
SRC = $(CLIENT_SRC) $(SERVER_SRC) $(COMMON_SRC)

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)
COMMON_OBJ = $(COMMON_SRC:.c=.o)
OBJ = $(CLIENT_OBJ) $(SERVER_OBJ) $(COMMON_OBJ)

all: client server
client: $(CLIENT_OBJ) $(COMMON_OBJ)
server: $(SERVER_OBJ) $(COMMON_OBJ)

clean:
	rm -rf $(OBJ) client server *~
