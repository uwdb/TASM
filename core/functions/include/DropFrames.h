//
// Created by Maureen Daum on 2019-06-26.
//

// Naive way to drop frames that don't contain a particular object.
// Read in boxes we care about.
// Go through video frame-by-frame and output frames that contain one of the objects we care about.

#ifndef LIGHTDB_DROPFRAMES_H
#define LIGHTDB_DROPFRAMES_H

#include "Functor.h"
#include "MaterializedLightField.h"
#include "Rectangle.h"
#include <fstream>
#include <iostream>

#include "timer.h"

// "/home/maureen/dog_videos/dog_labels/dog_dog_labels.boxes"

namespace lightdb {
class DropFrames : public functor::unaryfunctor {
    class GPU : public functor::unaryfunction {
    public:
        GPU()
            : functor::unaryfunction(physical::DeviceType::GPU, lightdb::Codec::raw(), true),
            frameIndex_(0),
            numberOfFramesWithObject_(0),
            shouldRemoveFrames_(true)
        {
            std::filesystem::path boxesPath = "/home/maureen/uadetrac_videos/MVI_20011/labels/bicycle_mvi_20011_boxes.boxes";

            Timer timer;
            timer.startSection();

            std::vector<Rectangle> rectangles(std::move(loadRectangles_(boxesPath)));

            std::vector<bool> framesWithObject(rectangles[rectangles.size()-1].id, false);
            std::for_each(rectangles.begin(), rectangles.end(), [&](const Rectangle &rectangle) {
                if (!framesWithObject[rectangle.id])
                    numberOfFramesWithObject_++;

                framesWithObject[rectangle.id] = true;
            });

            framesWithObject_ = framesWithObject;

            timer.endSection();
            std::cout << "ANALYSIS DropFrames-GPU-set-up took: " << timer.totalTimeInMillis() << " ms\n";
        }

        shared_reference<LightField> operator()(LightField &input) override {
            auto &data = dynamic_cast<physical::GPUDecodedFrameData&>(input);

            physical::GPUDecodedFrameData output{data.configuration(), data.geometry()};

            for (auto &frame: data.frames()) {
                bool frameContainsObject = false;
                if (frameIndex_ < framesWithObject_.size())
                    frameContainsObject = framesWithObject_[frameIndex_];

                frameIndex_++;

                if (shouldRemoveFrames_) {
                    if (frameContainsObject) {
                        output.frames().emplace_back(frame);
                    }
                } else {
                    if (!frameContainsObject) {
                        auto cuda = frame->cuda();
                        CHECK_EQ(cuMemsetD2D8(cuda->handle(),
                                              cuda->pitch(),
                                              0,
                                              cuda->width(),
                                              cuda->height()), CUDA_SUCCESS);
                    }
                }
            }

            return shouldRemoveFrames_ ? output : data;
        }

    private:
        std::vector<Rectangle> loadRectangles_(const std::filesystem::path &path) {
            std::ifstream ifs(path, std::ios::binary);

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
            return rectangles;
        }

        std::vector<bool> framesWithObject_;
        unsigned int frameIndex_;
        unsigned int numberOfFramesWithObject_;
        bool shouldRemoveFrames_;
    };

    class CPU : public functor::unaryfunction {
    public:
        CPU() : CPU("/home/maureen/uadetrac_videos/MVI_20011/labels/bus_mvi_20011_boxes.boxes") { }
        CPU(const std::filesystem::path &pathToBoxes)
            : lightdb::functor::unaryfunction(physical::DeviceType::CPU, lightdb::Codec::raw(), true),
            frameIndex_(0),
            numberOfFramesWithObject_(0),
            shouldRemoveFrames_(true),
            frameWidth_(0),
            frameHeight_(0)
        {
            Timer timer;
            timer.startSection();

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

            std::vector<bool> framesWithObject(rectangles[size-1].id, false);
            std::for_each(rectangles.begin(), rectangles.end(), [&](const Rectangle &rectangle) {
                if (!framesWithObject[rectangle.id])
                    numberOfFramesWithObject_++;

                framesWithObject[rectangle.id] = true;
            });

            framesWithObject_ = framesWithObject;

            timer.endSection();
            std::cout << "ANALYSIS DropFrames-CPU-set-up took: " << timer.totalTimeInMillis() << " ms\n";
        }

        shared_reference<LightField> operator()(LightField &input) override {
            auto &data = dynamic_cast<physical::CPUDecodedFrameData&>(input);
            physical::CPUDecodedFrameData output(data.configuration(), data.geometry());

            for (auto& frame: data.frames()) {
                if (!frameWidth_) {
                    frameWidth_ = frame->width();
                    frameHeight_ = frame->height();
                }

                if (frameIndex_ >= framesWithObject_.size()) {
                    if (!shouldRemoveFrames_) {
                        bytestring droppedFrameData(frame->data());
                        memset(droppedFrameData.data(), 0, frame->width() * frame->height());
                        output.frames().push_back(std::move(LocalFrameReference::make<LocalFrame>(*frame, droppedFrameData)));
                        ++frameIndex_;
                        continue;
                    } else
                        break;
                }

                if (framesWithObject_[frameIndex_])
                    output.frames().emplace_back(frame);
                else {
                    if (!shouldRemoveFrames_) {
                        bytestring droppedFrameData(frame->data());
                        memset(droppedFrameData.data(), 0, frame->width() * frame->height());
                        output.frames().push_back(std::move(LocalFrameReference::make<LocalFrame>(*frame, droppedFrameData)));
                    }
                }
                ++frameIndex_;
            }

            return output;
        }

        void handleAllDataHasBeenProcessed() override {
            std::cout << "ANALYSIS-VIDEO Total number of processed frames: " << frameIndex_
                << ", number of frames with object: " << numberOfFramesWithObject_
                << ", removed frames without objects: " << shouldRemoveFrames_
                << ", frame width: " << frameWidth_
                << ", frame height: " << frameHeight_
                << std::endl;
        }

    private:
        std::vector<bool> framesWithObject_;
        unsigned int frameIndex_;
        unsigned int numberOfFramesWithObject_;
        bool shouldRemoveFrames_;
        unsigned int frameWidth_;
        unsigned int frameHeight_;
    };

public:
    DropFrames() : functor::unaryfunctor("DropFrames", GPU()) { }
};

static DropFrames DropFrames{};
}


#endif //LIGHTDB_DROPFRAMES_H
