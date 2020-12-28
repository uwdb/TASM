# Instructions for building YOLO for the GPU operator.

From https://github.com/AlexeyAB/darknet#how-to-use-yolo-as-dll-and-so-libraries

1. `git clone https://github.com/AlexeyAB/darknet.git`
1. Edit the Makefile so `GPU=1`, `LIBSO=1`
1. Edit the Make file so `NVCC=/usr/local/cuda/bin/nvcc`
1. `make`
1. A `libdarknet.so` library should have been created.
1. Modify the path to the darknet repository in `CMakeLists.txt` under "# GPU Yolo linking".
