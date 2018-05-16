#!/usr/bin/env bash

# a shell script that uses sed to replace the <base> attribute in a directory of HTML files
# the directory is searched recursively for files that match '*.html'


usage()
{
   echo "$0 <dir> <href>"
   echo ""
   echo "  dir   target directory to replace <base> in"
   echo "  href  the new href target in <base>"
}

[ $# -eq 0 ] && { usage; exit 1; }

find $1 -name '*.html' | xargs sed --regexp-extended --in-place "/<base href=\"(.*)\">/c\<base href=\"${2}\">"
