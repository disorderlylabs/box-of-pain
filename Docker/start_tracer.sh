#!/bin/bash
docker run \
    --cap-add=SYS_PTRACE \
    --name="tracer" \
    --rm \
    -it \
    -v type=bind,source=$(pwd)/tmp,target=/tmp \
    --entrypoint \
    ./painbox painbox -T
