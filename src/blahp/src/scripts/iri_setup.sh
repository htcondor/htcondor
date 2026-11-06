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
# Copyright (c) HTCondor Team, Computer Sciences Department,
#   University of Wisconsin-Madison, WI. 2024.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [ -n "${iri_venv}" ] ; then
    if [ ! -f "${iri_venv}/bin/activate" ]; then
        echo "ERROR: IRI virtual environment not found at ${iri_venv}" >&2
        echo "Create it with: python3 -m venv ${iri_venv} && ${iri_venv}/bin/pip install authlib" >&2
        exit 1
    fi
    . "${iri_venv}/bin/activate"
fi

if [[ -n "$blah_debug" ]]; then
  # BLAH debug enabled in /etc/blah.config
  if [[ ! -d "${blah_debug}" ]]; then
    mkdir -p "${blah_debug}"
  fi
  blah_iri_log() { echo "*@" >> "${blah_debug}"/iri.log; }
  [[ -z "$BLAH_HELPER_LOGGING" ]] && export BLAH_HELPER_LOGGING="DEBUG;False;${blah_debug}/iri_helper.log"
else
  blah_iri_log() { :; }
fi

if [[ -n "$blah_debug_save_submit_info" ]]; then
  # BLAH save submit files and credentials enabled in /etc/blah.config
  # Making a subdirectory (multiple BLA helpers could be used at the same time)
  # This must be cleaned manually
  blah_debug_save_submit_info="${blah_debug_save_submit_info}/iri"
  if [[ ! -d "${blah_debug_save_submit_info}" ]]; then
    mkdir -p "${blah_debug_save_submit_info}"
  fi
  bls_iri_scripts_log() { echo "*@" >> "${blah_debug_save_submit_info}"/scripts.log; }
  # bls_iri_debug() { echo "*@" >> "${blah_debug_save_submit_info}"/debug.log; }
  [[ -z "$BLAH_HELPER_LOGGING" ]] && export BLAH_HELPER_LOGGING="DEBUG;True;${blah_debug_save_submit_info}/iri_helper.log"
else
  bls_iri_scripts_log() { :; }
  # bls_iri_debug() { :; }
fi

# Workaround for SCHEDD_ENVIRONMENT HTCondor variable not found
BLAHPD_IRI_SERVICE="${BLAHPD_IRI_SERVICE:-api.iri.nersc.gov,compute,scratch}"
BLAHPD_IRI_DIRECTORY="${BLAHPD_IRI_DIRECTORY:-/global/homes/m/marco/test}"