#!/bin/bash
docker run --cap-add=SYS_PTRACE --rm -it --security-opt seccomp:unconfined --pid=container:tracer --net tracing --ip 10.0.0.21 painbox
