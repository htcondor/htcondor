/**
 * Copyright 2015 University of Southern California
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <libgen.h>
#include <sys/stat.h>

#include "common.h"

#define SECONDS 1000
#define MINUTES (60*SECONDS)
#define TIMEOUT (1*MINUTES)

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s SOCKPATH\n\n", argv0);
    fprintf(stderr, "Where SOCKPATH is the path to the unix domain socket "
                    "that should be created\n");
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc != 2) {
        fprintf(stderr, "ERROR Invalid argument\n");
        usage();
        exit(1);
    }

    char *sockpath = argv[1];

    log(stderr, "%s starting...\n", argv0);
    log(stderr, "UNIX socket: %s\n", sockpath);

    unlink(sockpath);

    int sck = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sck < 0) {
        log(stderr, "ERROR creating socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, sockpath);
    socklen_t addrlen = strlen(local.sun_path) + sizeof(local.sun_family);

    if (bind(sck, (struct sockaddr *)&local, addrlen) < 0) {
        log(stderr, "ERROR binding socket: %s\n", strerror(errno));
        return 1;
    }

    if (listen(sck, 0) < 0) {
        log(stderr, "ERROR listening on socket: %s\n", strerror(errno));
        return 1;
    }

    /* Get the INODE number of the socket so we can check it later */
    struct stat st;
    if (stat(sockpath, &st) < 0) {
        log(stderr, "ERROR stat()ing socket: %s\n", strerror(errno));
        return 1;
    }
    ino_t sock_inode = st.st_ino;

    /* Initally poll to accept and check stdin */
    struct pollfd ufds[2];
    int client = -1;
    while (1) {
        ufds[0].fd = sck;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;
        ufds[1].fd = STDIN_FILENO;
        ufds[1].events = POLLHUP|POLLERR;
        ufds[1].revents = 0;

        int rv = poll(ufds, 2, TIMEOUT);
        if (rv == -1) {
            log(stderr, "ERROR polling for connection/stdin close: %s\n", strerror(errno));
            exit(1);
        } else if (rv == 0) {
            /* Check to make sure our socket file still exists */
            struct stat st;
            if (stat(sockpath, &st) < 0) {
                log(stderr, "ERROR stat()ing socket: %s\n", strerror(errno));
                exit(1);
            }
            if (sock_inode != st.st_ino) {
                log(stderr, "ERROR UNIX socket replaced\n");
                exit(1);
            }
        } else {
            if (ufds[0].revents & POLLIN) {
                struct sockaddr_un remote;
                client = accept(sck, (struct sockaddr *)&remote, &addrlen);
                if (client < 0) {
                    log(stderr, "ERROR accepting connection: %s\n", strerror(errno));
                    exit(1);
                }
                break;
            }
            if (ufds[0].revents != 0 && ufds[0].revents != POLLHUP) {
                log(stderr, "ERROR on UNIX listening socket!\n");
                exit(1);
            }
            if (ufds[1].revents != 0) {
                log(stderr, "ERROR stdin from SSH closed\n");
                exit(1);
            }
        }
    }

    /* Close the server socket before connecting I/O so the next proxy process
     * can start listening. */
    close(sck);

    /* Remove the socket here */
    unlink(sockpath);

    /* Handle all the I/O between client and server */
    int bytes_read = 0;
    char buf[BUFSIZ];
    while (1) {
        ufds[0].fd = client;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;
        ufds[1].fd = STDIN_FILENO;
        ufds[1].events = POLLIN;
        ufds[1].revents = 0;

        int rv = poll(ufds, 2, -1);
        if (rv == -1) {
            log(stderr, "ERROR polling UNIX socket/stdin: %s\n", strerror(errno));
            exit(1);
        } else {
            if (ufds[0].revents & POLLIN) {
                bytes_read = recv(client, buf, BUFSIZ, 0);
                if (bytes_read == 0) {
                    /* Proxy disconnected, should get POLLHUP */
                } else if (bytes_read < 0) {
                    log(stderr, "ERROR reading data from client socket: %s\n", strerror(errno));
                    exit(1);
                } else {
                    if (write(STDOUT_FILENO, buf, bytes_read) < 0) {
                        /* If that write failed, then this log message probably
                         * will too :-/ */
                        log(stderr, "ERROR writing to SSH (stdout): %s\n", strerror(errno));
                        exit(1);
                    }
                }
            }
            if (ufds[0].revents & POLLHUP) {
                log(stderr, "Client disconnected (socket closed)\n");
                exit(1);
            }
            if (ufds[1].revents & POLLIN) {
                bytes_read = read(STDIN_FILENO, buf, BUFSIZ);
                if (bytes_read == 0) {
                    /* SSH closed stdin, should get POLLHUP */
                } else if (bytes_read < 0) {
                    log(stderr, "ERROR reading from SSH/stdin: %s\n", strerror(errno));
                    exit(1);
                } else {
                    if (send(client, buf, bytes_read, 0) < 0) {
                        log(stderr, "ERROR sending message to client: %s\n", strerror(errno));
                        exit(1);
                    }
                }
            }
            if (ufds[1].revents & POLLHUP) {
                /* Likely will never be seen */
                log(stderr, "SSH disconnected (stdin closed)\n");
                exit(1);
            }
        }
    }

    close(client);

    return 0;
}

