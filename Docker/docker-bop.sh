#!/bin/bash

# sample file
# run to get into dockerland

# used so you can run it from wherever
# and remove the /mnt for WSL compatability reasons
path=`realpath $(dirname $0)/..`

docker run \
    --cap-add=SYS_PTRACE \
    -it --rm \
    --mount type=bind,source=${path#/mnt}/tmp,target=/tmp \
    painbox
