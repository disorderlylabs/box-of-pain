#!/bin/bash
docker run \
    --cap-add=SYS_PTRACE \
    -it \
    --rm \
    -v type=bind,source="$(pwd)"/tmp,target=/tmp \
    --entrypoint \
    ./painbox painbox -d -e ./client -e ./server
