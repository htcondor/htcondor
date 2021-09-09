#!/bin/bash

# filter out requirements that come from condor-external-libs

$(rpm --eval %__find_requires) "$@" | egrep -v '^lib(globus|vomsapi)' || :

