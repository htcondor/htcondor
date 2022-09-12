#!/bin/bash

name=htcondor/hpc-annex-pilot
tag=el8
hub=hub.opensciencegrid.org

docker login ${hub}  &&
docker tag ${name}:${tag} ${hub}/${name}:${tag}  &&
docker push ${hub}/${name}:${tag}

