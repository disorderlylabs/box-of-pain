Track system calls through a tree and determine when to ABSOLUTELY WRECK A DISTRIBUTED APPLICATION LETS DO THIS.

To compile: Tested on gcc-6 and above (I know it doesn't work on gcc-4). It requires Linux to run and compile.

To run a test program:
./painbox -e ./server -e ./client
This starts the server and the client. You may specify as many "-e program" flags as you like. Specify -d to produce the out.m4 file.

To make the out.m4 file into a PDF, run:
m4 out.m4 > out.dot && dot -Tpdf -o out.pdf out.dot

