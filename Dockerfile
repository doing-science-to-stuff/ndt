FROM debian

ENV CXX /usr/bin/gcc

RUN apt-get -y update
RUN apt-get -y install libpng-tools libjpeg-turbo-progs libyaml-0-2
RUN apt-get -y install cmake libpng-dev libjpeg-dev libyaml-dev
RUN apt-get -y install libmpich-dev mpich
# cmake seems to be missing a dependency for g++
RUN apt-get -y install g++

# using strace requires --cap-add SYS_PTRACE
#RUN apt-get -y install strace

COPY . /app
WORKDIR /app
RUN cmake .
RUN make

RUN apt-get -y --auto-remove remove cmake libpng-dev libjpeg-dev libyaml-dev
RUN apt-get -y --auto-remove remove libmpich-dev
RUN apt-get -y --auto-remove remove g++

#CMD /app/ndt -s /app/scenes/random.so
CMD /bin/bash

