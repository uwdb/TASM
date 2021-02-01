FROM tasm/environment
MAINTAINER Maureen Daum "mdaum@cs.washington.edu"

ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility,video

RUN apt-get update
RUN apt-get install \
        -y \
        --no-install-recommends \
        sqlite3 \
        libsqlite3-dev \
        ssh

COPY . /tasm

RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make
RUN cd python
