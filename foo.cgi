#!/bin/bash
#
# Parse the query string from the web server, such
# that each GET option ends up as $var_X, where
# X is the name of the option.
#
saveIFS=$IFS
IFS='=&'
parm=($QUERY_STRING)
IFS=$saveIFS
for ((i=0; i<${#parm[@]}; i+=2))
do
  declare var_${parm[i]}=${parm[i+1]}
done
#
# Process callback and jsonp options
#
if [[ -n "$var_callback" ]]; then jsonp="$var_callback"; fi
if [[ -n "$var_jsonp" ]]; then jsonp="$var_jsonp"; fi
#
# Process file option; default to file "foo.json" if not specified
#
if [[ -n "$var_file" ]]; then file="$var_file"; else file="foo"; fi
file=`basename $file`
file=${file//[^a-zA-Z0-9]/}.json
#
# Output HTTP header
#
echo "Content-Type: text/plain"
echo "Cache-Control:public, max-age=10"
echo "Last-Modified:" `TZ=GMT ls -l --time-style="+%a, %d %b %Y %T %Z" "$file" | cut -d' ' -f6-11`
echo
#
# Output content, converting JSON to JSONP if requested
#
if [[ -n "$jsonp" ]]; then echo "$jsonp"'(['; fi
cat "$file"
if [[ -n "$jsonp" ]]; then echo ']);'; fi
