#!/bin/bash
docker volume rm tracees_vol  >/dev/null &2>&1
docker run --cap-add=SYS_PTRACE --rm -it \
	--mount type=bind,source=$(pwd)/tmp,target=/tmp \
	--mount source=tracees_vol,destination=/tracees \
	--name=tracer painbox 
