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
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <libgen.h>
#include <netdb.h>
#include <signal.h>
#include <math.h>
#include <fcntl.h>

#include "common.h"

char *argv0 = NULL;

void sigterm(int sig) {
    log(stderr, "Recieved SIGTERM\n");
    exit(1);
}

void usage() {
    fprintf(stderr, "Usage: %s [-d] [-l LOGFILE]\n", argv0);
    fprintf(stderr, "  -d          Daemonize\n");
    fprintf(stderr, "  -l LOGFILE  Write logging messages to LOGFILE\n");
}

int set_condor_config() {
    char *homedir = getenv("HOME");
    if (homedir == NULL) {
        fprintf(stderr, "ERROR HOME is not set in environment");
        return -1;
    }

    char config_file[BUFSIZ];
    snprintf(config_file, BUFSIZ, "%s/%s", homedir, ".rvgahp/condor_config.rvgahp");
    if (access(config_file, R_OK) < 0) {
        fprintf(stderr, "ERROR Cannot find config file %s", config_file);
        return -1;
    }

    setenv("CONDOR_CONFIG", config_file, 1);
    return 0;
}

int _condor_config_val(char *var, char *val, size_t valsize, const char *default_val) {
    char cmd[BUFSIZ];
    snprintf(cmd, BUFSIZ, "condor_config_val %s 2>&1", var);

    FILE *proc = popen(cmd, "r");
    if (proc == NULL) {
        fprintf(stderr, "ERROR getting %s config var\n", var);
        return -1;
    }

    char buf[BUFSIZ];
    if (fgets(buf, BUFSIZ, proc) == NULL) {
        fprintf(stderr, "ERROR reading output of condor_config_val\n");
        pclose(proc);
        return -1;
    }

    int status = pclose(proc);
    int saverrno = errno;

    if (strncmp("Not defined:", buf, 12) == 0) {
        if (default_val == NULL) {
            return -1;
        }
        snprintf(val, valsize, "%s", default_val);
        return 0;
    }

    if (status != 0) {
        fprintf(stderr, "ERROR reading condor_config_val %s: %s\n", var, strerror(saverrno));
        fprintf(stderr, "%s", buf);
        return -1;
    }

    /* Trim the string */
    strtok(buf, "\r\n");

    snprintf(val, valsize, "%s", buf);

    return 0;
}

int condor_config_val(char *var, char *val, size_t valsize, const char *default_val) {
    /* Need to restore the default handler for this function so that wait4()
     * and, as a result, pclose() work correctly. */
    void *sigchld_handler = signal(SIGCHLD, SIG_DFL);
    int result = _condor_config_val(var, val, valsize, default_val);
    signal(SIGCHLD, sigchld_handler);
    return result;
}

int loop() {
    int socks[2];

    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, socks) < 0) {
        log(stderr, "ERROR socketpair failed: %s\n", strerror(errno));
        exit(1);
    }

    int gahp_sock = socks[0];
    int ssh_sock = socks[1];

    log(stdout, "Starting SSH connection\n");
    pid_t ssh_pid = fork();
    if (ssh_pid == 0) {
        int orig_err = dup(STDERR_FILENO);
        close(gahp_sock);
        close(0);
        close(1);
        dup(ssh_sock);
        dup(ssh_sock);
        execl("/bin/sh", "/bin/sh", "-c", "~/.rvgahp/rvgahp_ssh", NULL);
        dprintf(orig_err, "ERROR execing ssh script\n");
        _exit(1);
    } else if (ssh_pid < 0) {
        log(stderr, "ERROR forking ssh script\n");
        exit(1);
    }

    /* Close here so that if the remote process dies, our read returns */
    close(ssh_sock);

    /* Get name of GAHP to launch */
    log(stdout, "Waiting for request\n");
    char gahp[BUFSIZ];
    ssize_t b = read(gahp_sock, gahp, BUFSIZ);
    if (b < 0) {
        log(stderr, "ERROR read from SSH failed: %s\n", strerror(errno));
        /* This probably happened because the SSH process died */
        goto error;
    } else if (b == 0) {
        log(stderr, "ERROR SSH socket closed\n");
        goto error;
    } else {
        gahp[b] = '\0';
    }

    /* Trim the message */
    strtok(gahp, "\r\n");

    /* Construct the actual GAHP command */
    char gahp_command[BUFSIZ];
    if (strncmp("batch_gahp", gahp, 10) == 0) {
        char batch_gahp[BUFSIZ]; 
        if (condor_config_val("BATCH_GAHP", batch_gahp, BUFSIZ, NULL) < 0) {
            goto error;
        }
        char glite_location[BUFSIZ];
        if (condor_config_val("GLITE_LOCATION", glite_location, BUFSIZ, NULL) < 0) {
            goto error;
        }
        snprintf(gahp_command, BUFSIZ, "GLITE_LOCATION=%s %s", glite_location, batch_gahp);
    } else if (strncmp("condor_ft-gahp", gahp, 14) == 0) {
        char ft_gahp[BUFSIZ];
        if (condor_config_val("FT_GAHP", ft_gahp, BUFSIZ, NULL) < 0) {
            goto error;
        }
        snprintf(gahp_command, BUFSIZ, "%s -f", ft_gahp);
    } else {
        dprintf(gahp_sock, "ERROR: Unknown GAHP: %s\n", gahp);
        goto error;
    }
    log(stdout, "Actual GAHP command: %s\n", gahp_command);

    pid_t gahp_pid = fork();
    if (gahp_pid == 0) {
        int orig_err = dup(STDERR_FILENO);
        close(0);
        close(1);
        close(2);
        dup(gahp_sock);
        dup(gahp_sock);
        dup(gahp_sock);
        execl("/bin/sh", "/bin/sh", "-c", gahp_command, NULL);
        dprintf(orig_err, "ERROR execing GAHP\n");
        _exit(1);
    } else if (gahp_pid < 0) {
        log(stderr, "ERROR launching GAHP\n");
        exit(1);
    }

    close(gahp_sock);
    return 0;

error:
    close(gahp_sock);
    return 1;
}

int main(int argc, char** argv) {
    argv0 = basename(strdup(argv[0]));

    /* Parse arguments */
    char *logfilename = NULL;
    int daemonize = 0;
    int c;
    while ((c = getopt (argc, argv, "dl:")) != -1) {
        switch (c) {
        case 'd':
            daemonize = 1;
            break;
        case 'l':
            logfilename = optarg;
            break;
        case '?':
            if (optopt == 'l') {
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            } else {
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            }
            usage();
            return 1;
        default:
            abort();
        }
    }

    if (argc > optind) {
        fprintf(stderr, "Invalid argument: %s\n", argv[optind]);
        usage();
        exit(1);
    }

    /* Set up configuration */
    if (set_condor_config() < 0) {
        exit(1);
    }

    /* Redirect stdio to logfile if specified */
    if (logfilename != NULL) {
        int logfile = open(logfilename, O_WRONLY|O_CREAT|O_APPEND, 0600);
        if (logfile < 0) {
            fprintf(stderr, "ERROR opening logfile %s: %s\n", logfilename, strerror(errno));
            exit(1);
        }
        close(1);
        close(2);
        dup(logfile);
        dup(logfile);
    }

    if (daemonize) {
        setsid();
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "ERROR forking: %s\n", strerror(errno));
            exit(1);
        } else if (pid == 0) {
            /* Child goes on */
        } else {
            /* Parent exits */
            exit(0);
        }
    }

    log(stdout, "%s starting...\n", argv0);
    log(stdout, "Config file: %s\n", getenv("CONDOR_CONFIG"));

    signal(SIGCHLD, SIG_IGN);
    signal(SIGTERM, sigterm);

    int failures = 0;
    while (1) {
        if (loop() == 0) {
            failures = 0;
        } else {
            failures++;
            unsigned int sleeptime = 300;
            if (failures < 9) {
                sleeptime = (unsigned int)pow(2, failures);
            }
            log(stderr, "Failure occured, waiting %u seconds to respawn\n", sleeptime);
            sleep(sleeptime);
        }
    }

    exit(0);
}

