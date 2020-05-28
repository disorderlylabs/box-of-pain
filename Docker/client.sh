#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --pid=container:tracer --net tracing \
	--mount source=tracees_vol,target=/tracees \
	--entrypoint ./painbox painbox -C examples/client-server/client 
