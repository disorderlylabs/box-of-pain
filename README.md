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

