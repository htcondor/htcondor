#!/usr/bin/bash
#
# File:     iri_setup.sh
#
# Description:
#   The iri scripts rely on the Python package authlib.
#   If variable iri_venv is set in the blahp.config file, that means
#   the package is available in a Python virtual environment under the
#   given path, and we need to activate it.
#
#   Source this file (do not execute it) from other iri_*.sh scripts
#   before invoking iri_helpers.py.
#
#   You can create the virtual environment with these commands:
#     python3 -m venv ${iri_venv}
#     ${iri_venv}/bin/pip install authlib
#

if [ ! -z "${iri_venv}" ] ; then
    if [ ! -f "${iri_venv}/bin/activate" ]; then
        echo "ERROR: IRI virtual environment not found at ${iri_venv}" >&2
        echo "Create it with: python3 -m venv ${iri_venv} && ${iri_venv}/bin/pip install authlib" >&2
        exit 1
    fi

    . "${iri_venv}/bin/activate"
fi
