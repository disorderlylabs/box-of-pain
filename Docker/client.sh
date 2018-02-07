#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --pid=container:tracer --net tracing --ip 10.0.0.22 --entrypoint ./painbox painbox -C ./client "10.0.0.21"
