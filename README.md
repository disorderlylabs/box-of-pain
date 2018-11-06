Track system calls through a tree and determine when to ABSOLUTELY WRECK A DISTRIBUTED APPLICATION LETS DO THIS.

To compile: Tested on gcc-6 and above (I know it doesn't work on gcc-4). It REQUIRES Linux to run (and maybe compile).

Usage: painbox [-d] [-e program,arg1,arg2...]...
    -d: Dump result into out.m4 and out.inc
    -e: Trace a program. The argument to -e is the program you want to trace,
        followed by arguments (separated by commas). See below for an example. More complex arguments may not work (like arguments
        that themselves have commas, or whatever. Add support for that if you want that).

To run a test program:
./painbox -e ./server -e ./client
Q-server:
./painbox -e ./quorum_server -e ./client -e ./client -e ./client

To make the out.m4 file into a PDF, run:
m4 out.m4 > out.dot && dot -Tpdf -o out.pdf out.dot


Tracing, Following, and Stepping, Oh My!
----------------------------------------
To serialize a run, add to the execution commands: `-s test.boprun`




Research Questions / Work
-------------------------
More complex examples? Do they work? Do they work correctly? What does the graph
output look like?

What do these graphs looks like with different programming languages or
networking libraries? Does it work at all?

*Fix the system if the above things don't work. This may be as simple as
implementing more system calls and transport-layer protocols (such as UDP).
It may be as difficult as fixing bugs in the implementation.*

Right now we just support instrumenting different processes. We would like to be
able to instrument processes in different docker containers. Is this possible?
How do we do it?

What is the performance impact of our system? How much does it impact
connection-mode and connection-less sockets? Overhead on connect? Overhead on
throughput and latency for communication? This needs some thought on how to do
it best.

*Can we improve performance? I have some ideas...*


