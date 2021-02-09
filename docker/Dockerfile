FROM tasm/environment
MAINTAINER Maureen Daum "mdaum@cs.washington.edu"

ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility,video


COPY . /tasm

RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make && \
    cd python/python_package && \
    python3 setup.py sdist bdist_wheel && \
    pip3 install -e .

WORKDIR /tasm/python/Examples
