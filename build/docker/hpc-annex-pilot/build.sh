#!/bin/bash

name=htcondor/hpc-annex-pilot
tag=el8

docker build -t ${name}:${tag} .

