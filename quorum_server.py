import socket, sys

s = socket.socket()
host = socket.gethostname()
print "host is " + str(host)
port = 1234
s.bind(("127.0.0.1",port))
s.listen(5)


votes = 0

while True:
    print "call accept"
    c, addr = s.accept()
    print("Connection accepted from " + repr(addr[1]))
    msg = c.recv(128)
    print "msg " + msg
    c.send("Server approved connection\n")
    print repr(addr[1]) + ": " + c.recv(1026)

    votes += 1

    c.close()

    if votes >= 3:
	print "OUT"
	sys.exit(0)
