#!/bin/bash
#
# File:     sfapi_setup.sh
#
# Description:
#   Activates the Python virtual environment that contains the sfapi_client
#   package and its dependencies. Source this file (do not execute it) from
#   other sfapi_*.sh scripts before invoking sfapi_helpers.py.
#
#   The virtual environment is expected at ~/superfacility-env.
#   Create it once with:
#     python3 -m venv ~/superfacility-env
#     ~/superfacility-env/bin/pip install sfapi_client
#

sfapi_venv="${HOME}/superfacility-env"

if [ ! -f "${sfapi_venv}/bin/activate" ]; then
    echo "ERROR: SFAPI virtual environment not found at ${sfapi_venv}" >&2
    echo "Create it with: python3 -m venv ${sfapi_venv} && ${sfapi_venv}/bin/pip install sfapi_client" >&2
    exit 1
fi

. "${sfapi_venv}/bin/activate"
