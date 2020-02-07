#!/bin/bash

prog=${0##*/}
progdir=${0%/*}

fail () {
    echo "$prog:" "$@" >&2
    exit 1
}

# Create a config file from the environment.
# The config file needs to be on disk instead of referencing the env
# at run time so condor_config_val can work.
printf "%s=%s\n" > /etc/condor/config.d/01-env.conf \
    CONDOR_HOST "${CONDOR_HOST:-\$(FULL_HOSTNAME)}" \
    NUM_CPUS "${NUM_CPUS:-1}" \
    MEMORY "${MEMORY:-1024}" \
    RESERVED_DISK "${RESERVED_DISK:-1024}" \
    USE_POOL_PASSWORD "${USE_POOL_PASSWORD:-no}"

"$progdir/update-secrets"
"$progdir/update-config"

# The master will crash if run as pid 1 (bug?) plus supervisor can restart
# it if it dies, and gives us the ability to run other services.

exec /usr/bin/supervisord -c /etc/supervisord.conf

# vim:et:sw=4:sts=4:ts=8
