#!/usr/bin/env bash

echo "----------------"
echo "1K Input"

echo "20 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-1K-20s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "40 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-1K-40s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "60 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-1K-60s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "----------------"
echo "2K Input"

echo "20 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-2K-20s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "40 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-2K-40s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "60 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-2K-60s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "----------------"
echo "4K Input"

echo "20 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-4K-20s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "40 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-4K-40s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

echo "60 seconds"
ffmpeg -hide_banner -loglevel error -y -i ../../../test/resources/test-4K-60s.h264 -c copy input.mp4
time ./opencv_map input.mp4 

rm input.mp4
rm out.mp4
