FROM alpine
RUN apk update
RUN apk add build-base
WORKDIR /box-of-pain
COPY . /box-of-pain
RUN make
CMD /bin/sh
