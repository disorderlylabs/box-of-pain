#!/bin/bash

path=$(pwd)

#./painbox -e examples/client-server/client -e examples/client-server/server -d /tmp/

docker run \
    --cap-add=SYS_PTRACE \
    -it --rm \
    --mount type=bind,source=${path#/mnt}/tmp,target=/tmp \
    painbox
