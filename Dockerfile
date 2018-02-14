FROM alpine
RUN apk update
RUN apk add build-base
RUN mkdir /tracees
WORKDIR /box-of-pain
COPY . /box-of-pain
RUN make
CMD /bin/sh
