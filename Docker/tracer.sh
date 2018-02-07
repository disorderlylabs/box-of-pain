#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --mount type=bind,source=$(pwd)/tmp,target=/tmp --name=tracer --net tracing --ip 10.0.0.20  painbox
