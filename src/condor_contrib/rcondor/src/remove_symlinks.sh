#!/bin/bash

#
# Args: bindir listfile
#

if [ $# -ne 2 ]; then
  echo "Requires 2 args: bindir listfile" >&2
  exit 1
fi


bdir="$1"

if [ ! -d "$bdir" ]; then
   echo "bin dir does not exist: $bdir"
   exit 2
fi

lfile="$2"

while read line
do
  if [ "$line" == "" ]; then
   continue
  fi

  if [ "${line:0:1}" == "#" ]; then
    continue
  fi

  tfile="$bdir/$line"
  rm "$tfile"
  echo "$tfile"
done < "$lfile"


