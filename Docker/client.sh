#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --pid=container:tracer --net tracing --mount source=tracees-vol,target=/tracees --entrypoint ./painbox painbox -C ./client 
