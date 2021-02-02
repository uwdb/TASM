FROM tasm/environment
MAINTAINER Maureen Daum "mdaum@cs.washington.edu"

ENV NVIDIA_VISIBLE_DEVICES all
ENV NVIDIA_DRIVER_CAPABILITIES compute,utility,video


COPY . /tasm

RUN mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Debug .. && make

# TODO: Run installation as part of CMake
RUN python3 -m pip install --upgrade pip && \
    python3 -m pip install setuptools wheel jupyter notebook opencv-python pandas && \
    python3 -m pip install --upgrade nbconvert && \
    cd python/python_package && \
    python3 setup.py sdist bdist_wheel && \
    pip3 install -e .

RUN apt-get update && apt-get install -y libopencv-dev python-opencv

WORKDIR /tasm/python/Examples
