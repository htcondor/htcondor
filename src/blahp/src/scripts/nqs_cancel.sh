#!/bin/bash

. `dirname $0`/blah_load_config.sh

jnr=0
jc=0
for job in  $@ ; do
        jnr=$(($jnr+1))
done
for  job in  $@ ; do
        requested=`echo $job | sed 's/^.*\///'`
        ${nqs_binpath}qdel -k $requested >/dev/null 2>&1
        if [ "$?" == "0" ] ; then
                if [ "$jnr" == "1" ]; then
                        echo " 0 No\\ error"
                else
                        echo .$jc" 0 No\\ error"
                fi
        else
                if [ "$jnr" == "1" ]; then
                        echo " 1 Error"
                else
                        echo .$jc" 1 Error"
                fi
        fi
        jc=$(($jc+1))
done

