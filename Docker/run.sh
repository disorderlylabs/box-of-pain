#!/bin/bash

path=`realpath $(dirname $0)/..`

docker run \
    --cap-add=SYS_PTRACE \
    --rm \
    -it \
    --mount type=bind,source=${path#/mnt}/tmp,target=/tmp \
    --entrypoint \
    ./painbox painbox -e ./examples/client-server/client -e ./examples/client-server/server