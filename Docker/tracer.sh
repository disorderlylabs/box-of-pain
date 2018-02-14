#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it \
	--mount type=bind,source=$(pwd)/tmp,target=/tmp \
	--mount source=tracees,destination=/tracees \
	--name=tracer painbox
