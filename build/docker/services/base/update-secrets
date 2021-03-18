#!/bin/bash

prog=${0##*/}

fail () {
    echo "$prog:" "$@" >&2
    exit 1
}


# Copy secrets mounted at /root/secrets (old way; not recommended since
# it doesn't let you have multiple passwords/tokens)

# Pool password; used for PASSWORD auth or for the collector to generate tokens from.
if [[ -f /root/secrets/pool_password ]]; then
    umask 077
    install -o root -g root -m 0600 -D /root/secrets/pool_password /etc/condor/passwords.d/POOL ||\
        fail "/root/secrets/pool_password found but unable to copy"
    # Create example tokens (in root's home dir so they don't mess anything up)
    condor_token_create -auth ADVERTISE_MASTER -auth ADVERTISE_SCHEDD -auth READ -identity dockersubmit@example.net > /root/dockersubmit-example-token
    condor_token_create -auth ADVERTISE_MASTER -auth ADVERTISE_STARTD -auth READ -identity dockerworker@example.net > /root/dockerworker-example-token
    umask 022
fi

# A token.
if [[ -f /root/secrets/token ]]; then
    umask 077
    install -o condor -g condor -m 0600 -D /root/secrets/token /etc/condor/tokens.d/token ||\
        fail "/root/secrets/token found but unable to copy"
    umask 022
fi



# Copy mounted secrets as follows:

# /etc/condor/tokens-orig.d/*                  -> /etc/condor/tokens.d
# /etc/condor/passwords-orig.d/*               -> /etc/condor/passwords.d
# /etc/grid-security-orig.d/host{cert,key}.pem -> /etc/grid-security
# Only files get copied.
# XXX This does not delete any files from the destination, only overwrites existing ones.

copy_secrets_in_dir () {
    local from="$1"
    local to="$2"
    local owner="$3"
    shopt -s nullglob
    if [[ -d $from ]]; then
        for file in "$from"/*; do
            if [[ -f $file ]]; then
                install -o "$owner" -g "$owner" -m 0600 "$file" "$to"/"$(basename "$file")"
            fi
        done
    fi
    shopt -u nullglob
}

# Only root needs to know the pool password but before 8.9.12, the condor user
# also needed to access the tokens.
condor_version=$(condor_config_val CONDOR_VERSION)
if python3 -c '
import sys
version = [int(x) for x in sys.argv[1].split(".")]
sys.exit(0 if version >= [8, 9, 12] else 1)
' "$condor_version"; then
    copy_secrets_in_dir /etc/condor/tokens-orig.d  /etc/condor/tokens.d  root
else
    copy_secrets_in_dir /etc/condor/tokens-orig.d  /etc/condor/tokens.d  condor
fi
copy_secrets_in_dir /etc/condor/passwords-orig.d  /etc/condor/passwords.d  root

if [[ -f /etc/grid-security-orig.d/hostcert.pem && -f /etc/grid-security-orig.d/hostkey.pem ]]; then
    install -o root -g root -m 0644 /etc/grid-security-orig.d/hostcert.pem /etc/grid-security/hostcert.pem
    install -o root -g root -m 0600 /etc/grid-security-orig.d/hostkey.pem  /etc/grid-security/hostkey.pem
fi


# vim:et:sw=4:sts=4:ts=8
