#!/usr/bin/env python
import socket, sys

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
host = socket.gethostname()
print("host is " + str(host))
port = 1234
s.bind(("127.0.0.1",port))
s.listen(5)


votes = 0

while True:
    print("call accept")
    c, addr = s.accept()
    print("Connection accepted from " + repr(addr[1]))
    msg = c.recv(128)
    print("msg " + str(msg))
    c.send(b"Server approved connection\n")
    print(str(repr(addr[1])) + ": " + str(c.recv(1026)))

    votes += 1

    c.close()

    if votes >= 3:
        print("OUT")
        sys.exit(0)
