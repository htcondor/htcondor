#!/bin/sh

ln -s stdsoap2_2.7.6b.c stdsoap2.c
ln -s stdsoap2_2.7.6b.h stdsoap2.h
head stdsoap2.h | perl -ne '/^\s*stdsoap2.h\s+([0-9])\.([0-9])\.(\S+)\s.*/ && printf "#define GSOAP_VERSION %d%02d%02d\n#define GSOAP_MIN_VERSION \"$3\"\n#ident \"soap_version.h $1.$2.$3\"\n",$1,$2,$3' >soap_version.h

