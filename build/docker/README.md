# HTCondor Dockerfiles

This directory contains Dockerfiles for an HTCondor build image (can be
used for building HTCondor) and service images (can be used for running the
HTCondor daemons.)

## HTCondor Build Dockerfiles

The centos7/ directory contains Dockerfiles to create images on CentOS 7
containing all the dependencies needed to build all the HTCondor 
packages, tarballs, and documentation from source, as released on htcondor.org.


### Building HTCondor Inside a Container

No matter which of the below methods you'll use, you should start by running
```shell
docker pull htcondor/build-<platform>
```
where `<platform>` is the platform you want to build for (e.g., `centos7`).

You may also want to append a tag for a specific version of HTCondor.
For example, to pull the build container for HTCondor version 8.9.3 on CentOS 7,
you would run
```shell
docker pull htcondor/build-centos7:V8_9_3
```

The same convention can be used directly in the `docker run` commands below if
you prefer not to pre-fetch the image.

Throughout the rest of these examples, we'll assume that you're building the
absolute latest version of HTCondor (i.e., whatever is on `master`) for CentOS 7.
Replace the image name appropriately if you are targeting some other platform.


#### Building Without Local Source

If you don't have a local clone of the HTCondor git repository, the container
can automatically fetch one for you.
This is useful for one-off builds, or more generally for when you don't plan on
editing any source code.
For this method, simply run
```shell 
docker run -it --rm htcondor/build-centos7
```
After a few moments, you'll be in the container, sitting above the HTCondor source. 

TODO: build instructions


#### Building From Local Source

If you already have a local clone of the HTCondor git repository, you can 
bind-mount it into the container and build from it.
To do so, use a `docker run` command that looks like this, from the root of your
local HTCondor repository:
```shell
docker run -it --rm --mount type=bind,source="$PWD",target=/home/build/htcondor,readonly htcondor/build-centos7
```
This will leave you in the container, sitting above the HTCondor source.
From there you can configure and run the build as normal.


### Extracting Build Artifacts from the Container

You may need to get some build artifact (like a tarballed release directory)
out of the container and back onto your host machine.
To do so, detach from the container (try pressing `ctrl+p` followed by `ctrl+q`),
then run (from a shell on the host machine)
```shell
docker cp <container_name>:/home/build/<tarball_name> .
```
The `<container_name>` is Docker identifier for the container (from `docker container ls`).
The `<tarball_name>` is the name of the file inside the container that you'd like to pull out 
(obviously, you can replace that part of the command with whatever you'd like).
Note the trailing `.`: this is the path where Docker will put the extracted file 
(in this case, the current working directory).


### Working on the Documentation

The build container includes `sphinx-autobuild` for convenient editing of the
documentation.
`sphinx-autobuild` automatically rebuilds the documentation when it notices
changes, and can also serve built HTML docs via a built-in web server.
This is important because without it, it would be difficult to edit the parts of
the manual that depend on having a built HTCondor (i.e., the Python bindings).

We recommend writing a small script that looks like this, for use inside the 
container:
```bash
#!/usr/bin/env bash

set -e

export PYTHONPATH=$HTCONDOR_RELEASE_DIR/lib/python3:$PYTHONPATH
export LD_LIBRARY_PATH=$HTCONDOR_RELEASE_DIR/lib:$LD_LIBRARY_PATH

sphinx-autobuild --host 0.0.0.0 --port 8000 "$HTCONDOR_REPO_DIR/docs/" ~/docs
```
This will start the webserver and give you a link to click on.
For that link to work, you must bind a port on the container when you run it by
adding `-p 8000:8000` to your `docker run` arguments.


## HTCondor Service Dockerfiles

These are Dockerfiles and supporting files in the `services/` directory tree.
See `services/README.md` for information.
