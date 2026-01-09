#!/bin/bash

. `dirname $0`/blah_load_config.sh

requested=`echo $1 | sed 's/^.*\///'`
${nqs_binpath}qrls $requested >/dev/null 2>&1
