FROM alpine
RUN apk update
RUN apk add build-base gdb
RUN mkdir /tracees
WORKDIR /output
COPY . /box-of-pain
RUN make -C /box-of-pain
CMD /bin/sh
