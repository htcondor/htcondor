#!/bin/bash
cmd=`basename "$0"`
conf=$HOME/.rcondor/rcondor.conf

if [ ! -f $conf ]; then
  echo "rcondor: config file does not exist: $conf" >&2  
  exit 1
fi

# parse config file
usr_host=`awk -F = '/^ *USR_HOST *=/ {print $2}' "$conf"`
usr_host=`echo $usr_host` # strip whitespaces
local=`awk -F = '/^ *LOCAL *=/ {print $2}' "$conf"`
local=`echo $local`
remote=`awk -F = '/^ *REMOTE *=/ {print $2}' "$conf"`
remote=`echo $remote`

# make sure config values are nonzero length
if [ -z "$usr_host" ] || [ -z "$local" ] || [ -z "$remote" ]; then
  echo "rcondor: error parsing $conf" >&2
  exit 1
fi

if ! pwd | grep -q "^${local}"; then
  echo "rcondor: working directory outside of mount point: $local" >&2
  exit 1
fi

wdir=`pwd | sed "s|${local}|${remote}|g"`

# wrap each argument in quotes to preserve whitespaces
for arg in "$@"; do
  args=$args"'$arg' "
done

ssh $usr_host "cd '$wdir' && $cmd $args"
