HTCondor Containers
===================

We provide the following containers for HTCondor services:
- Minicondor (`htcondor/mini`)
- Execute Node (`htcondor/execute`)
- Central Manager (`htcondor/cm`)
- Submit Node (Access Point) (`htcondor/submit`)



Using the Minicondor Container
------------------------------

### Overview

The minicondor container is an install with all of the HTCondor daemons
running, only listening on local interfaces.  This is useful for
experimentation and learning.

Start the container by running:
```console
dockerhost$ docker run --detach \
                --name=minicondor \
                htcondor/mini:el7
```
Then, enter the container by running:
```console
dockerhost$ docker exec -ti minicondor /bin/bash
```

You can submit jobs by first becoming the `submituser` user:
```console
container$ su - submituser
```


A simple way to add extra configuration to the minicondor container is to
volume-mount a file to `/etc/condor/condor_config.local`.  For example,
if you have a file called `condor_config.local` in your current directory,
run docker like this:
```console
dockerhost$ docker run --detach \
                --name=minicondor \
                -v `pwd`/condor_config.local:/etc/condor/condor_config.local \
                htcondor/mini:el7
```
See the [Providing Additional
Configuration](#providing-additional-configuration) section below for more
ways to add extra configuration.


### RESTD

The minicondor container includes the experimental REST interface to HTCondor,
[HTCondor-RESTD](https://github.com/htcondor/htcondor-restd).

The RESTD runs on startup on port 8080.  To access it from outside the
container, add `-p 8080:8080` to the `docker run` command above -- for example:
```console
dockerhost$ docker run --detach \
                -p 8080:8080 \
                --name=minicondor \
                htcondor/mini:el7
```
You can send queries to the RESTD via `localhost:8080`.  See the
[RESTD documentation](https://github.com/htcondor/htcondor-restd#readme)
for more details.



Using the Execute Node Container
--------------------------------

### Overview

The execute node container can connect to an existing HTCondor pool via
token authentication (recommended) or pool password.  Token authentication
requires HTCondor 8.9.2+ on both sides of the connection.  Because of CCB, this
container needs no inbound connectivity, and only needs outbound connectivity
to the central manager and the submit host, even for running jobs or using
`condor_ssh_to_job`.

You must specify the address of the pool's central manager in the `CONDOR_HOST`
environment variable or `CONDOR_SERVICE_HOST` environment variable.

You must also configure security -- see the [Configuring Access and
Security](#configuring-access-and-security) section below.
If you are using token auth, you will need a token with an identity that
the central manager accepts, with the following privileges:
-   `READ`
-   `ADVERTISE_MASTER`
-   `ADVERTISE_STARTD`

Here are the environment variables that the container can use:
- `CONDOR_HOST`/`CONDOR_SERVICE_HOST` (required): the address of the central
  manager
- `NUM_CPUS` (optional, default 1): the number of CPUs to advertise
- `MEMORY` (optional, default 1024): the amount of total memory (in MB)
  to advertise
- `RESERVED_DISK` (optional, default 1024): the amount of disk space
  (in MB) to reserve for non-HTCondor processes
- `USE_POOL_PASSWORD` (optional, default no): set this to `yes` to use
  pool password authentication

To add additional configuration, see the [Providing Additional
Configuration](#providing-additional-configuration) section below.


### Example

This example is for creating an execute node and adding it to a pool using
token auth.  The execute node will have one partitionable slot with 2 CPUs
and 4 GB of RAM, and will use the identity `dockerworker@example.net`.
The central manager has the hostname `cm.example.net`.  The container is
run by the user `user` on the host `dockerhost.example.net`.

Create a directory for holding the token:
```console
dockerhost$ mkdir -p ~/condorexec/secrets
dockerhost$ chmod 0700 ~/condorexec/secrets
```

Grant access to the identity that the container will use.  On the central
manager, add the following lines to the HTCondor configuration:
```
ALLOW_ADVERTISE_MASTER = \
    $(ALLOW_ADVERTISE_MASTER) \
    $(ALLOW_WRITE_COLLECTOR) \
    dockerworker@example.net
ALLOW_ADVERTISE_STARTD = \
    $(ALLOW_ADVERTISE_STARTD) \
    $(ALLOW_WRITE_COLLECTOR) \
    dockerworker@example.net
```
Run `condor_reconfig` on the central manager to pick up the changes.

Create a token for the execute node.  On the central manager:
```console
cm$ sudo condor_token_create -authz ADVERTISE_MASTER \
         -authz ADVERTISE_STARTD -authz READ -identity dockerworker@example.net \
         -token dockerworker_token
cm$ sudo scp dockerworker_token user@dockerhost.example.net:volumes/condorexec/secrets/token
```

On the Docker host, create an environment file for the container:
```console
dockerhost$ echo 'CONDOR_HOST=cm.example.net' > ~/condorexec/env
dockerhost$ echo 'NUM_CPUS=2' >> ~/condorexec/env
dockerhost$ echo 'MEMORY=4096' >> ~/condorexec/env
```

Start the container:
```console
dockerhost$ docker run --detach --env-file=~/condorexec/env \
                -v ~/condorexec/secrets:/root/secrets:ro \
                --name=htcondor-execute \
                --cpus=2
                --memory-reservation=$(( 4096 * 1048576 )) \
                htcondor/execute:el7
```

To verify the container is functioning, use `docker ps` to get the name
of the container, then run:
```console
dockerhost$ docker exec -ti <container name> /bin/bash
container$ condor_status
```
You should see the container in the output of `condor_status`.


### Adding additional software to the image

To add additional software to the execute node, create your own Dockerfile
that uses the execute node image as a base, and installs the additional
software.  For example, to install numpy, put this in a Dockerfile:
```dockerfile
FROM htcondor/execute:8.9.5-el7
RUN \
    yum install -y numpy && \
    yum clean all && \
    rm -rf /var/cache/yum/*
```

and build with:
```console
$ docker build -t custom-htcondor-worker .
```

Afterwards, use `custom-htcondor-worker` instead of `htcondor/execute:el7`
in your `docker run` command.



Using the Central Manager Container
-----------------------------------

### Overview

The central manager (cm) container includes the collector and
negotiator daemons as described in [the machine roles section of the HTCondor
manual](https://htcondor.readthedocs.io/en/latest/admin-manual/introduction-admin-manual.html#the-different-roles-a-machine-can-play).
Other hosts in the HTCondor pool can connect to the central manager via
token authentication (recommended) or pool password.  Token authentication
requires HTCondor 8.9.2+ on both sides of the connection.

The central manager requires inbound connectivity on port 9618.

You must also configure security -- see the [Configuring Access and
Security](#configuring-access-and-security) section below.
If you are using token auth, you will need to the master password file --
this is the secret used to create new tokens and validate existing ones.

Here are the environment variables that the container can use:
- `USE_POOL_PASSWORD` (optional, default no): set this to `yes` to use
  pool password authentication

To add additional configuration, see the [Providing Additional
Configuration](#providing-additional-configuration) section below.


Using the Submit (Access Point) Container
-----------------------------------------

### Overview

The submit container (htcondor/submit) includes the
scheduler daemon as described in [the machine roles section of the HTCondor
manual](https://htcondor.readthedocs.io/en/latest/admin-manual/introduction-admin-manual.html#the-different-roles-a-machine-can-play)
and can connect to an existing HTCondor pool via token authentication
(recommended) or pool password.  Token authentication requires HTCondor 8.9.2+
on both sides of the connection.  This container requires inbound connectivity
on port 9618.

You must specify the address of the pool's central manager in the `CONDOR_HOST`
environment variable or `CONDOR_SERVICE_HOST` environment variable.

You must also configure security -- see the [Configuring Access and
Security](#configuring-access-and-security) section below.
If you are using token auth, you will need a token with an identity that the
central manager accepts, with the following privileges:
-   `READ`
-   `ADVERTISE_MASTER`
-   `ADVERTISE_SCHEDD`

Here are the environment variables that the container can use:
- `CONDOR_HOST`/`CONDOR_SERVICE_HOST` (required): the address of the central
  manager
- `USE_POOL_PASSWORD` (optional, default no): set this to `yes` to use
  pool password authentication

To add additional configuration, see the [Providing Additional
Configuration](#providing-additional-configuration) section below.


### Submitting jobs

In order to submit jobs from the submit container, you must become the
`submituser` user.  Interactively:
```console
root@container# su - submituser
submituser@container$ condor_submit job.sub
```


Providing Additional Configuration
----------------------------------

You can add more HTCondor configuration in two ways:
- Adding `/root/config/*.conf` files.
- Adding an `/etc/condor/condor_config.local` file.

If you change files in `/root/config`, you will need to run `/update-config`
to copy the changes to the proper directory.  `/update-config` will also
run `condor_reconfig` for you.

If you change `/etc/condor/condor_config.local`, you will need to run
`condor_reconfig` yourself.

To perform additional pre-startup tasks, add a bash script to
`/root/config/pre-exec.sh`.  It will be run just before daemons are launched
(i.e. before `supervisord` starts).


Configuring Access and Security
-------------------------------

You can use either token auth (IDTOKEN) or pool password auth (PASSWORD)
Pool password is useful for testing but we recommend using token auth
when possible.  Note that token auth will only be used if both sides of
the connection are version 8.9.2 or newer.

To use token auth only:
-   Mount a directory with one or more token files at
    `/etc/condor/tokens-orig.d`.

To also enable pool password auth:
-   Specify `USE_POOL_PASSWORD=yes` in the container environment.
-   Mount a directory with one or more password files at
    `/etc/condor/passwords-orig.d`.

Note: token auth will be preferred over pool password if tokens are
available.

The `/update-secrets` script (also run at startup) will copy password files
to `/etc/condor/passwords.d` and token files to `/etc/condor/tokens.d`,
with the appropriate ownership and permissions set.

If you make any changes to secrets, run `/update-secrets` afterward or
restart the container to properly install the new secrets.

Note: Deleting a token file from `/etc/condor/tokens-orig.d` or a password
file from `/etc/condor/passwords-orig.d` does not delete the corresponding
file from `/etc/condor/tokens.d` or `/etc/condor/passwords.d`.

Compatibility note:  you can also put a pool password file into
`/root/secrets/pool_password` and a token into `/root/secrets/token`.
This method only lets you have one password file and one token file.


Building
--------

The images are built and pushed using the `build-images` script.


The images for the various roles (`execute`, `cm`, `mini`, `submit`) are built
on top of the `htcondor/base` image, which gets built first.  `htcondor/base`
contains the HTCondor software, some common configuration and scripts.

To build (but not push) a complete set of images for the latest version
on the default Linux distribution, run:
```console
$ ./build-images
```

Alternatively, you can build images for a specific version with
```console
$ ./build-images --version=<VERSION>
```
(where `<VERSION>` is an HTCondor version number, like `8.9.11`).

You can build daily images with:
```console
$ ./build-images --version=daily
```

Build for different distributions with the `--distro` argument:
```console
$ ./build-images --distro=el8
```

Push the results with `--push`; you can optionally specify a different
registry with `--registry`:
```console
$ ./build-images --push --registry=registry.example.net
```

Run build-images with `--help` to see additional options and defaults.


Known Issues
------------

- Changes to the volumes mounted at `/root/config` and `/root/secrets`
  do not get noticed automatically.  After updating config, run
  `/update-config` in the container.  After updating secrets, run
  `/update-secrets` in the container.

- cgroups support is not yet implemented.

- Singularity support is not yet implemented.
