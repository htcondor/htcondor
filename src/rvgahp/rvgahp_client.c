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
#include <sys/poll.h>
#include <libgen.h>
#include <sys/un.h>

char *argv0 = NULL;

void usage() {
    fprintf(stderr, "Usage: %s SOCKPATH GAHP_NAME\n\n", argv0);
    fprintf(stderr, "Where SOCKPATH is the path to the unix domain socket\n"
                    "that should be created, and GAHP_NAME is 'batch_gahp'\n"
                    "or 'condor_ft-gahp'\n");
}

int main(int argc, char **argv) {
    argv0 = basename(strdup(argv[0]));

    if (argc != 3) {
        fprintf(stderr, "Wrong number of arguments: expected 3, got %d\n", argc);
        usage();
        exit(1);
    }

    char *sockpath = argv[1];
    char *gahp = argv[2];

    /* If the socket doesn't exist, give it 30 seconds to appear */
    int tries = 0;
    while (access(sockpath, R_OK|W_OK) != 0) {
        tries++;
        if (tries < 30) {
            sleep(1);
        } else {
            fprintf(stderr, "ERROR No UNIX socket\n");
            return 1;
        }
    }

    int proxy = socket(AF_UNIX, SOCK_STREAM, 0);
    if (proxy < 0) {
        fprintf(stderr, "ERROR creating socket: %s\n", strerror(errno));
        return 1;
    }

    struct sockaddr_un remote;
    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, sockpath);
    socklen_t addrlen = strlen(remote.sun_path) + sizeof(remote.sun_family);

    if (connect(proxy, (struct sockaddr *)&remote, addrlen) < 0) {
        fprintf(stderr, "ERROR connecting to proxy socket: %s\n", sockpath);
        exit(1);
    }

    /* Tell the remote site the GAHP we want to start */
    if (dprintf(proxy, "%s\r\n", gahp) < 0) {
        fprintf(stderr, "ERROR sending GAHP message to proxy: %s\n", strerror(errno));
        close(proxy);
        exit(1);
    }

    /* Handle all the I/O between GridManager and proxy */
    int bytes_read = 0;
    char buf[BUFSIZ];
    struct pollfd ufds[2];
    while (1) {
        ufds[0].fd = proxy;
        ufds[0].events = POLLIN;
        ufds[0].revents = 0;
        ufds[1].fd = STDIN_FILENO;
        ufds[1].events = POLLIN;
        ufds[1].revents = 0;

        int rv = poll(ufds, 2, -1);
        if (rv == -1) {
            fprintf(stderr, "ERROR polling socket and stdin: %s\n", strerror(errno));
            exit(1);
        } else {
            if (ufds[0].revents & POLLIN) {
                bytes_read = recv(proxy, buf, BUFSIZ, 0);
                if (bytes_read == 0) {
                    /* Helper closed connection, should get POLLHUP */
                } else if (bytes_read > 0) {
                    if (write(STDOUT_FILENO, buf, bytes_read) < 0) {
                        fprintf(stderr, "ERROR writing data to GridManager (stdout): %s\n", strerror(errno));
                        break;
                    }
                } else {
                    fprintf(stderr, "ERROR reading from proxy socket: %s\n", strerror(errno));
                    break;
                }
            }
            if (ufds[0].revents & POLLHUP) {
                fprintf(stderr, "Proxy hung up (socket disconnected)\n");
                break;
            }

            if (ufds[1].revents & POLLIN) {
                bytes_read = read(STDIN_FILENO, buf, BUFSIZ);
                if (bytes_read == 0) {
                    /* GridManager closed stdin, should get POLLHUP */
                } else if (bytes_read > 0) {
                    if (send(proxy, buf, bytes_read, 0) < 0) {
                        fprintf(stderr, "ERROR sending data to proxy socket: %s\n", strerror(errno));
                        break;
                    }
                } else {
                    fprintf(stderr, "ERROR reading from GridManager (stdin)\n");
                    break;
                }
            }
            if (ufds[1].revents & POLLHUP) {
                /* Likely won't be seen */
                fprintf(stderr, "GridManager hung up (stdin closed)\n");
                break;
            }
        }
    }

    close(proxy);

    return 0;
}

