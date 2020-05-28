#!/bin/bash


docker run \
    --cap-add=SYS_PTRACE \
    -it \
    --rm \
    --mount type=bind,source="$(pwd)"/output,target=/output \
    --entrypoint \
    /box-of-pain/painbox painbox -d -e /box-of-pain/examples/client-server/client -e /box-of-pain/examples/client-server/server
