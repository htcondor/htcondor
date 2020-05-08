#!/bin/bash

export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
export FLASK_APP=condor_restd
export FLASK_ENV=dev

exec /usr/bin/flask-3 run -h 0.0.0.0 -p 8080
