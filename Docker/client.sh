#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --pid=container:tracer --net tracing --entrypoint ./painbox painbox -C ./client 
