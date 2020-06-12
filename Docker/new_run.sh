#!/bin/bash


docker run \
    --cap-add=SYS_PTRACE \
    -it \
    --rm \
    --mount type=bind,source="$(pwd)"/output,target=/output \
    --entrypoint \
    ./painbox painbox -d -e examples/client-server/client -e examples/client-server/server
