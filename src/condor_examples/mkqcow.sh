#!/bin/bash

# This script should call qemu-img to create a qcow image based on its
# first line of input.  Basically, it should take img_name.img and
# create a qcow in the same directory as that image, and then echo the
# location of that qcow.

read
export qcow_cnt=`ls $REPLY* | wc -l`
qemu-img create -f qcow2 -b $REPLY $REPLY.$qcow_cnt.qcow >/dev/null
echo $REPLY.$qcow_cnt.qcow
