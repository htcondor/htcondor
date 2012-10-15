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

if [[ "$cmd" == "rcondor_id" ]]; then 
  cmd="id"
fi

# wrap each argument in quotes to preserve whitespaces
for arg in "$@"; do
  args=$args"'$arg' "
done

mydir=`readlink -e $PWD`
if ! echo "$mydir" | grep -q "^${local}"; then
  if [[ "$cmd" = "condor_submit" || "$cmd" = "condor_submit_dag" || "$cmd" = "condor_userlog" ]]; then 
    echo "$cmd can only be run from inside the mountpoint: $localraw" >&2
    exit 1
  fi

  $ssh "$usr_host" "$cmd $args"
else
  wdir=`echo "$mydir" | sed "s|${local}|${remote}|g"`

  $ssh "$usr_host" "cd '$wdir' && $cmd $args"
fi
