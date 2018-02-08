#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --mount type=bind,source=$(pwd)/tmp,target=/tmp --name=tracer painbox
