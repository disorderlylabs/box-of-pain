FROM ubuntu
RUN apt-get update
RUN apt-get --yes install build-essential
WORKDIR /box-of-pain
COPY . /box-of-pain
RUN make
CMD /bin/sh
