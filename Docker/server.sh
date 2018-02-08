#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --pid=container:tracer --net=tracing --name=server --entrypoint ./painbox painbox -C ./server
