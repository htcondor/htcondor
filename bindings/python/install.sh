#!/bin/bash
set -ex

PYTHON=$1
SETUP_PY=$2
PREFIX=$3
LIB_DIR=$4

if [ ! -z "$DESTDIR" ]; then
    echo "Prepending PREFIX with DESTDIR"
    PREFIX=${DESTDIR}/${PREFIX}
fi

$PYTHON $SETUP_PY install --root=$PREFIX --install-lib=$LIB_DIR
