//
// Created by Maureen Daum on 2019-06-27.
//

#ifndef LIGHTDB_SELECTPIXELS_H
#define LIGHTDB_SELECTPIXELS_H

#include "Functor.h"
#include "Rectangle.h"
#include <fstream>
#include <iostream>

//#include <boost/icl/interval_base_set.hpp>

// "/home/maureen/uadetrac_videos/MVI_20011/labels/bus_mvi_20011_boxes.boxes"

namespace lightdb {
class SelectPixels : public functor::unaryfunctor {
    class CPU : public functor::unaryfunction  {
    public:
        CPU() : CPU("/home/maureen/uadetrac_videos/MVI_20011/labels/bus_mvi_20011_boxes.boxes") { }
        CPU(const std::filesystem::path &pathToBoxes)
            : functor::unaryfunction(physical::DeviceType::CPU, Codec::raw(), true),
            frameIndex_(0),
            shouldRemoveFrames_(true),
            numberOfPixelsInBoxes_(0),
            frameWidth_(0),
            frameHeight_(0),
            numberOfFramesWithObject_(0)
        {
            Timer timer;
            timer.startSection("SelectPixelsSetUp");

            std::ifstream ifs(pathToBoxes, std::ios::binary);

            char numDigits;
            ifs.get(numDigits);
            std::string sizeAsString;
            for (int i = 0; i < (int)numDigits; i++) {
                char next;
                ifs.get(next);
                sizeAsString += next;
            }
            int size = std::stoi(sizeAsString);

            std::vector<Rectangle> rectangles(size);
            ifs.read(reinterpret_cast<char *>(rectangles.data()), size * sizeof(Rectangle));

            std::for_each(rectangles.begin(), rectangles.end(), [&](const Rectangle &rectangle) {
                frameToRectangles_[rectangle.id].push_back(std::move(rectangle));
                numberOfPixelsInBoxes_ += rectangle.width * rectangle.height;
            });

            timer.endSection("SelectPixelsSetUp");
            std::cout << "ANALYSIS SelectPixels-set-up took: " << timer.totalTimeInMillis("SelectPixelsSetUp") << " ms\n";
        }

        shared_reference<LightField> operator()(LightField &input) override {
            auto &data = dynamic_cast<physical::CPUDecodedFrameData&>(input);
            physical::CPUDecodedFrameData output(data.configuration(), data.geometry());

            for (auto &frame: data.frames()) {
                if (!frameWidth_) {
                    frameWidth_ = frame->width();
                    frameHeight_ = frame->height();
                }

                const std::vector<Rectangle> &rectanglesForFrame = frameToRectangles_[frameIndex_];
                frameIndex_++;

                bytestring frameData(frame->data());
                unsigned int width = frame->width();
                unsigned int height = frame->height();

                if (rectanglesForFrame.size())
                    numberOfFramesWithObject_++;
                else if (shouldRemoveFrames_)
                    continue;
                else {
                    // No objects in this frame, but we aren't pruning frames.
                    // Therefore, just set the entire frame to black and continue.
                    memset(frameData.data(), 0, width * height);
                    output.frames().push_back(std::move(LocalFrameReference::make<LocalFrame>(*frame, frameData)));
                    continue;
                }

                MinimumAndMaximums minAndMax = minimumAndMaximumYCoordinates(rectanglesForFrame);

                // memset from 0 to min.
                memset(frameData.data(), 0, width * minAndMax.minY);

                // memset from max to height.
                unsigned int rowBeyondMaxY = minAndMax.maxY + 1;
                memset(frameData.data() + width * rowBeyondMaxY, 0, width * (height - rowBeyondMaxY));

                // For each row in between, memset from 0->left and right->width
                // Set pixels in the middle one-by-one.
                unsigned int columnBeyondMaxX = minAndMax.maxX + 1;
                unsigned int lastRowToLookAt = std::min(minAndMax.maxY, height-1);
                for (unsigned int y = minAndMax.minY; y <= lastRowToLookAt; y++) {
                    memset(frameData.data() + width * y, 0, minAndMax.minX);
                    memset(frameData.data() + width * y + columnBeyondMaxX, 0, width - columnBeyondMaxX);
                    for (unsigned int x = minAndMax.minX; x <= minAndMax.maxX; x++) {
                        bool pointIsRelevant = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](const Rectangle &rectangle) {
                            return rectangle.containsPoint(x, y);
                        });

                        if (!pointIsRelevant)
                            frameData[y * width + x] = 0;
                    }
                }

                output.frames().push_back(std::move(LocalFrameReference::make<LocalFrame>(*frame, frameData)));
            }
            return output;
        }

        void handleAllDataHasBeenProcessed() override {
            std::cout << "ANALYSIS-VIDEO \"number_of_pixels_preserved\": " << numberOfPixelsInBoxes_
                << ", \"number_of_frames_with_objects\": " << numberOfFramesWithObject_
                << ", \"total_number_of_frames_in_og_video\": " << frameIndex_
                << ", \"frame_width\": " << frameWidth_
                << ", \"frame_height\": " << frameHeight_
                << ", \"pixels_per_frame\": " << frameWidth_ * frameHeight_
                << ", \"pixels_in_og_video\": " << frameWidth_ * frameHeight_ * frameIndex_
                << std::endl;
        }
    private:
        struct MinimumAndMaximums {
            unsigned int minY;
            unsigned int maxY;
            unsigned int minX;
            unsigned int maxX;

            MinimumAndMaximums()
                : minY(std::numeric_limits<unsigned int>::max()),
                maxY(0),
                minX(std::numeric_limits<unsigned int>::max()),
                maxX(0)
            { }
        };

        MinimumAndMaximums minimumAndMaximumYCoordinates(const std::vector<Rectangle> &rectangles) {
            MinimumAndMaximums minimumAndMaximums;

            std::for_each(rectangles.begin(), rectangles.end(), [&](const Rectangle &rectangle) {
                unsigned int top = rectangle.y;
                unsigned int bottom = top + rectangle.height;
                if (top < minimumAndMaximums.minY)
                    minimumAndMaximums.minY = top;
                if (bottom > minimumAndMaximums.maxY)
                    minimumAndMaximums.maxY = bottom;

                unsigned int left = rectangle.x;
                unsigned int right = left + rectangle.width;
                if (left < minimumAndMaximums.minX)
                    minimumAndMaximums.minX = left;
                if (right > minimumAndMaximums.maxX)
                    minimumAndMaximums.maxX = right;
            });

            return minimumAndMaximums;
        }

        unsigned int frameIndex_;
        std::unordered_map<unsigned int, std::vector<Rectangle>> frameToRectangles_;
        bool shouldRemoveFrames_;
        unsigned long numberOfPixelsInBoxes_;
        unsigned int frameWidth_;
        unsigned int frameHeight_;
        unsigned int numberOfFramesWithObject_;
    };

    // "/home/maureen/dog_videos/dog_labels/dog_dog_labels.boxes"
    // TODO: This can be parallelized by setting all of the rectangular regions to black.
    class GPU : public functor::unaryfunction {
    public:
        GPU() : GPU("/home/maureen/uadetrac_videos/MVI_20011/labels/person_mvi_20011_boxes.boxes") { }
        GPU(const std::filesystem::path &pathToBoxes)
            : functor::unaryfunction(physical::DeviceType::GPU, Codec::raw(), true),
            frameIndex_(0),
            shouldRemoveFrames_(false)
        {
            std::ifstream ifs(pathToBoxes, std::ios::binary);

            char numDigits;
            ifs.get(numDigits);
            std::string sizeAsString;
            for (int i = 0; i < (int)numDigits; i++) {
                char next;
                ifs.get(next);
                sizeAsString += next;
            }
            int size = std::stoi(sizeAsString);

            std::vector<Rectangle> rectangles(size);
            ifs.read(reinterpret_cast<char *>(rectangles.data()), size * sizeof(Rectangle));

            std::for_each(rectangles.begin(), rectangles.end(), [&](const Rectangle &rectangle) {
                frameToRectangles_[rectangle.id].push_back(std::move(rectangle));
            });
        }

        shared_reference<LightField> operator()(LightField &input) override {
            auto &data = dynamic_cast<physical::GPUDecodedFrameData&>(input);
            physical::GPUDecodedFrameData output{data.configuration(), data.geometry()};

            for (auto &frame: data.frames()) {
                // Find boxes on that frame.
                const std::vector<Rectangle> &rectanglesForFrame = frameToRectangles_[frameIndex_];
                ++frameIndex_;

                // Set pixels to black that are not in the frame.
                // To start with, set all pixels to black.
                auto cuda = frame->cuda();
                unsigned int width = frame->width();
                unsigned int height = frame->height();

                if (!rectanglesForFrame.size()) {
                    if (shouldRemoveFrames_) {
                        // Don't include this frame in the output.
                        continue;
                    } else {
                        // Set the frame to black.
                        CHECK_EQ(cuMemsetD2D8(cuda->handle(),
                                cuda->pitch(),
                                0,
                                width,
                                height), CUDA_SUCCESS);
                    }
                }

                if (rectanglesForFrame.size()) {
                    MinimumAndMaximums minAndMax = minimumAndMaximumYCoordinates(rectanglesForFrame);

                    // Set to black from top to first box.
                    CHECK_EQ(cuMemsetD2D8(cuda->handle(),
                            cuda->pitch(),
                            0,
                            width,
                            minAndMax.minY), CUDA_SUCCESS);

                    // Set to black from bottom of last box to bottom of frame.
                    unsigned int rowBeyondMaxY = minAndMax.maxY + 1;
                    if (rowBeyondMaxY < height) {
                        CHECK_EQ(cuMemsetD2D8(cuda->handle() + cuda->pitch() * rowBeyondMaxY,
                                              cuda->pitch(),
                                              0,
                                              width,
                                              height - rowBeyondMaxY), CUDA_SUCCESS);
                    }

                    unsigned int columnBeyondMaxX = minAndMax.maxX + 1;
                    unsigned int lastRowToLookAt = std::min(minAndMax.maxY, height-1);
                    for (unsigned int y = minAndMax.minY; y <= lastRowToLookAt; y++) {
                        CHECK_EQ(cuMemsetD8(cuda->handle() + cuda->pitch() * y,
                                0,
                                minAndMax.minX), CUDA_SUCCESS);

                        if (columnBeyondMaxX < width) {
                            CHECK_EQ(cuMemsetD8(cuda->handle() + cuda->pitch() * y + columnBeyondMaxX,
                                                0,
                                                width - columnBeyondMaxX
                            ), CUDA_SUCCESS);
                        }

                        for (unsigned int x = minAndMax.minX; x <= minAndMax.maxX; x++) {
                            bool pointIsRelevant = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](const Rectangle &rectangle) {
                                return rectangle.containsPoint(x, y);
                            });

                            if (!pointIsRelevant) {
                                CHECK_EQ(cuMemsetD8(cuda->handle() + cuda->pitch() * y + x,
                                        0,
                                        1), CUDA_SUCCESS);
                            }
                        }
                    }
                }
                if (shouldRemoveFrames_)
                    output.frames().emplace_back(frame);
            }

            return shouldRemoveFrames_ ? output : data;
        }
    private:
        struct MinimumAndMaximums {
            unsigned int minY;
            unsigned int maxY;
            unsigned int minX;
            unsigned int maxX;

            MinimumAndMaximums()
                    : minY(std::numeric_limits<unsigned int>::max()),
                      maxY(0),
                      minX(std::numeric_limits<unsigned int>::max()),
                      maxX(0)
            { }
        };

        MinimumAndMaximums minimumAndMaximumYCoordinates(const std::vector<Rectangle> &rectangles) {
            MinimumAndMaximums minimumAndMaximums;

            std::for_each(rectangles.begin(), rectangles.end(), [&](const Rectangle &rectangle) {
                unsigned int top = rectangle.y;
                unsigned int bottom = top + rectangle.height;
                if (top < minimumAndMaximums.minY)
                    minimumAndMaximums.minY = top;
                if (bottom > minimumAndMaximums.maxY)
                    minimumAndMaximums.maxY = bottom;

                unsigned int left = rectangle.x;
                unsigned int right = left + rectangle.width;
                if (left < minimumAndMaximums.minX)
                    minimumAndMaximums.minX = left;
                if (right > minimumAndMaximums.maxX)
                    minimumAndMaximums.maxX = right;
            });

            return minimumAndMaximums;
        }

        std::unordered_map<unsigned int, std::vector<Rectangle>> frameToRectangles_;
        unsigned int frameIndex_;
        bool shouldRemoveFrames_;
    };
public:
    SelectPixels() : functor::unaryfunctor("SelectPixels", GPU()) { }
};

static SelectPixels SelectPixels{};
} // namespace lightdb

#endif //LIGHTDB_SELECTPIXELS_H
