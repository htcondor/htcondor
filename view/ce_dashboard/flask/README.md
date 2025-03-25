# The CE Dashboard Web Server

The CE Dashboard is a web service that allows sites to view how much compute
capacity has been contributed and allocated via their CE to national
cyberinfrastructure pools like OSPool. 

Currently the CE Dashboard is limited to monitoring CE servers that are hosted
by OSG.  It consists of two components, typically run on two separate containers
and/or servers:

1. The **Database container**, which holds the time series database (e.g.
ganglia or prometheus), and an instance of the _condor_gangliad_ from HTCSS.

2. The **Web Server container**, which serves the CE Dashboard website, and is
the topic of this document.  

This directory holds the source code for the CE Dashboard web server.

## Requirements

The web server container needs to have incoming
HTTP network connectivity to handle network requests from browsers.  It also
needs to be able to make outgoing network connections to the database container.
It needs about 20GB of free disk space, but it does NOT need any persistent
volumes.  Amount of CPU and RAM depends on how popular we think this service will be, but starting out with 16 GB RAM and 4 cpu cores (and then setting GUNICORN_CPUS=8, see below) is an OK place to start.... of course more is better :).

## Deploying the Web Server container

The CE Dashboard Web Server is implemented in Python using Flask, a lightweight framework well-suited for building microservices, meaning it is
compatible with any web server platform that can handle WSGI (e.g. Apache
mod_wsgi, NGINX, waitress, gunicorn, etc). Gunicorn is included as the default in the container image due to its simplicity, performance, and compatibility with Flask applications.
It will expose one network port (default: port 5000) for incoming HTTP connections from browsers.  It is assumed that some other proxy will terminate SSL. 

### Building the Web Server container image

You will need `git` and `docker build` (or similar tool).  

First step is checkout the source code, which is stored in the HTCondor
repository on GitHub. If you do not already have the source code, you can use a
_sparse checkout_ to just grab the files needed for the web server via the
following commands:

```console
$ git clone --depth 1 --branch V24_X-HTCONDOR-2969-ce-dashboard --no-checkout https://github.com/htcondor/htcondor.git
$ cd htcondor
$ git sparse-checkout set view/ce_dashboard/flask
$ git checkout
$ cd view/ce_dashboard/flask
```
> [!IMPORTANT]
> Soon this code will be merged into the main branch, at which point the git
clone command option `--branch V24_X-HTCONDOR-2969-ce-dashboard` should be
removed.

Now that you have the source code and are in the `view/ce_dashboard/flask` subdirectory, you can build the container image:

```console
$ docker build -t ce_dashboard_web .
```

### Launching the Web Server container

Here are the environment variables that the container can use:

* **GUNICORN_CPUS** [optional, default=1+2*(CpuCores)]: Controls how many
CPU cores the gunicorn web server will utilize.  A reasonable choice is two
times the number of CPU cores assigned to the container.  The image will attempt
to determine the number of cores at runtime, but oftentimes it will detect the
number of physical cores on the host instead of the number of cores provisioned by
kubernetes - thus you may want to explicitly set this parameter.

* **CE_DASHBOARD_CACHE_DIRECTORY** [optional,
 default=`/tmp/ce_dashboard_cache`]: The directory where the cache files will
 be stored.  This directory should be writable by the web server user. If it
 does not exist, it will be created. This directory does not need to be on a
 persistent volume. Figure this directory should be able to hold about 20GB of
 files.

* **CE_DASHBOARD_SERVER_CACHE_MINUTES** [optional, default=20] The number of
 minutes to cache data on this server in the CE_DASHBOARD_CACHE_DIRECTORY.  The
 bigger the number, the less query load on the time series database, but the
 data will be more stale on the user's browser.

* **CE_DASHBOARD_BROWSER_CACHE_MINUTES** [optional, default=30] The number of
 minutes to cache data in the browser. The bigger the number, the more
 responsive the website will feel, but the data viewed will be more stale.

Finally, launch the container.  This example shows launching the web server, having it listen on port 5000, and telling it to use 8 cpu cores:

```[console]
docker run --rm -e GUNICORN_CPUS=8 -p 5000:5000 ce_dashboard_web
```

### References
* [Docker's Python guide](https://docs.docker.com/language/python/)
