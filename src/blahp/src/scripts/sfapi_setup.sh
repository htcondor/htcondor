#!/bin/bash
#
# File:     sfapi_setup.sh
#
# Description:
#   The sfapi scripts rely on the Python package sfapi_client.
#   If variable sfapi_venv is set in the blahp.config file, that means
#   the package is available in a Python virtual environment under the
#   given path, and we need to activate it.
#
#   Source this file (do not execute it) from other sfapi_*.sh scripts
#   before invoking sfapi_helpers.py.
#
#   You can create the virtual environment with these commands:
#     python3 -m venv ${sfapi_venv}
#     ${sfapi_venv}/bin/pip install sfapi_client
#

if [ ! -z "${sfapi_venv}" ] ; then
    if [ ! -f "${sfapi_venv}/bin/activate" ]; then
        echo "ERROR: SFAPI virtual environment not found at ${sfapi_venv}" >&2
        echo "Create it with: python3 -m venv ${sfapi_venv} && ${sfapi_venv}/bin/pip install sfapi_client" >&2
        exit 1
    fi

    . "${sfapi_venv}/bin/activate"
fi
