#!/bin/sh

trap `echo "SIG2"; exit;` 2
sleep 600
echo Past Sleep
