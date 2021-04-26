#!/bin/bash

prog=${0##*/}
progdir=${0%/*}

fail () {
    echo "$prog:" "$@" >&2
    exit 1
}

add_values_to () {
    config=$1
    shift
    printf "%s=%s\n" >> "/etc/condor/config.d/$config" "$@"
}

# Create a config file from the environment.
# The config file needs to be on disk instead of referencing the env
# at run time so condor_config_val can work.
echo "# This file was created by $prog" > /etc/condor/config.d/01-env.conf
add_values_to 01-env.conf \
    CONDOR_HOST "${CONDOR_SERVICE_HOST:-${CONDOR_HOST:-\$(FULL_HOSTNAME)}}" \
    NUM_CPUS "${NUM_CPUS:-1}" \
    MEMORY "${MEMORY:-1024}" \
    RESERVED_DISK "${RESERVED_DISK:-1024}" \
    USE_POOL_PASSWORD "${USE_POOL_PASSWORD:-no}"


bash -x "$progdir/update-secrets" || fail "Failed to update secrets"
bash -x "$progdir/update-config" || fail "Failed to update config"


# Bug workaround: daemons will die if they can't raise the number of FD's;
# cap the request if we can't raise it.
hard_max=$(ulimit -Hn)

rm -f /etc/condor/config.d/01-fdfix.conf
# Try to raise the hard limit ourselves.  If we can't raise it, lower
# the limits in the condor config to the maximum allowable.
for attr in COLLECTOR_MAX_FILE_DESCRIPTORS \
            SHARED_PORT_MAX_FILE_DESCRIPTORS \
            SCHEDD_MAX_FILE_DESCRIPTORS \
            MAX_FILE_DESCRIPTORS; do
    config_max=$(condor_config_val -evaluate $attr 2>/dev/null)
    if [[ $config_max =~ ^[0-9]+$ && $config_max -gt $hard_max ]]; then
        if ! ulimit -Hn "$config_max" &>/dev/null; then
            add_values_to 01-fdfix.conf "$attr" "$hard_max"
        fi
        ulimit -Hn "$hard_max"
    fi
done
[[ -s /etc/condor/config.d/01-fdfix.conf ]] && \
    echo "# This file was created by $prog" >> /etc/condor/config.d/01-fdfix.conf

# The master will crash if run as pid 1 (bug?) plus supervisor can restart
# it if it dies, and gives us the ability to run other services.

if [[ -f /root/config/pre-exec.sh ]]; then
    bash -x /root/config/pre-exec.sh
fi

exec /usr/bin/supervisord -c /etc/supervisord.conf

# vim:et:sw=4:sts=4:ts=8
