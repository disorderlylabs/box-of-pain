#!/bin/bash
docker run \
    --cap-add=SYS_PTRACE \
    --pid="container:tracer" \
    -it \
    -v type=bind,source=$(pwd)/tmp,target=/tmp \
    --command="./painbox painbox -C ./server && ./painbox -C ./client"
