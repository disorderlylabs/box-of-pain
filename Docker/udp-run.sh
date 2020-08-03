#!/bin/bash

path=`realpath $(dirname $0)/..`

#./painbox -e examples/udp-client-server/client -e examples/udp-client-server/server -d/tmp/

docker run \
    --cap-add=SYS_PTRACE \
    -it --rm \
    --mount type=bind,source=${path#/mnt}/tmp,target=/tmp \
    --entrypoint ./painbox painbox -e examples/udp-client-server/client -e examples/udp-client-server/server -d/tmp/
