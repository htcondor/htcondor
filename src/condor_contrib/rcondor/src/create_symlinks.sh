#!/bin/bash

#
# Args: bindir libexecdir listfile
# Note: libexecdir can be relative to bindir
#

if [ $# -ne 3 ]; then
  echo "Requires 3 args: bindir libexecdir listfile" >&2
  exit 1
fi


bdir="$1"

if [ ! -d "$bdir" ]; then
   echo "bin dir does not exist: $bdir"
   exit 2
fi

ldir="$2"

if [ ! -f "$ldir/rcondor.sh" ]; then
  if [ ! -f "$bdir/$ldir/rcondor.sh" ]; then 
    echo "libexec dir does not contain rcondor.sh: $ldir"
    exit 2
  fi
fi

lfile="$3"

while read line
do
  if [ "$line" == "" ]; then
   continue
  fi

  if [ "${line:0:1}" == "#" ]; then
    continue
  fi

  tfile="$bdir/$line"
  ln -s "$ldir/rcondor.sh" "$tfile"
  echo "$tfile"
done < "$lfile"


