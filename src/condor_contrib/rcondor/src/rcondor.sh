#!/bin/bash
real0=`readlink -e "$0"`
swdir=`dirname "$real0"`

confscript="$swdir/rcondor_config.source"
if [ ! -f "$confscript" ]; then
  echo -e "rcondor: improper installation, missing $confscript" >&2
  exit 2
fi

source "$confscript"

cmd=`basename "$0"`

if ! pwd | grep -q "^${local}"; then
  echo "rcondor: working directory outside of mount point: $local" >&2
  exit 1
fi

wdir=`pwd | sed "s|${local}|${remote}|g"`

# wrap each argument in quotes to preserve whitespaces
for arg in "$@"; do
  args=$args"'$arg' "
done

$ssh "$usr_host" "cd '$wdir' && $cmd $args"
