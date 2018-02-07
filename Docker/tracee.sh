#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --security-opt seccomp:unconfined --pid=container:tracer painbox
