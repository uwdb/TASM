//
// Created by Maureen Daum on 2019-06-26.
//

// Naive way to drop frames that don't contain a particular object.
// Read in boxes we care about.
// Go through video frame-by-frame and output frames that contain one of the objects we care about.

#ifndef LIGHTDB_DROPFRAMES_H
#define LIGHTDB_DROPFRAMES_H

#include "Functor.h"
#include "Rectangle.h"
#include <fstream>
#include <iostream>

namespace lightdb {
class DropFrames : public functor::unaryfunctor {
    class CPU : public functor::unaryfunction {
    public:
        CPU() : CPU("/home/maureen/dog_videos/dog_labels/dog_dog_labels.boxes") { }
        CPU(const std::filesystem::path &pathToBoxes)
            : lightdb::functor::unaryfunction(physical::DeviceType::CPU, lightdb::Codec::raw(), true),
            frameIndex_(0),
            numberOfDroppedFrames_(0)
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

            std::vector<bool> framesWithObject(rectangles[size-1].id, false);
            std::for_each(rectangles.begin(), rectangles.end(), [&](const Rectangle &rectangle) {
                framesWithObject[rectangle.id] = true;
            });

            framesWithObject_ = framesWithObject;
        }

        shared_reference<LightField> operator()(LightField &input) override {
            auto &data = dynamic_cast<physical::CPUDecodedFrameData&>(input);
            physical::CPUDecodedFrameData output(data.configuration(), data.geometry());

            for (auto& frame: data.frames()) {
                if (frameIndex_ >= framesWithObject_.size()) {
                    numberOfDroppedFrames_++;
                    break;
                }

                if (framesWithObject_[frameIndex_])
                    output.frames().emplace_back(frame);
                else
                    numberOfDroppedFrames_++;

                ++frameIndex_;
            }

            return output;
        }

        void handleAllDataHasBeenProcessed() override {
            std::cout << "Total number of processed frames: " << frameIndex_ << ", number of dropped frames: " << numberOfDroppedFrames_ << std::endl;
        }

    private:
        std::vector<bool> framesWithObject_;
        unsigned int frameIndex_;
        unsigned int numberOfDroppedFrames_;
    };

public:
    DropFrames() : functor::unaryfunctor("DropFrames", CPU()) { }
};

static DropFrames DropFrames{};
}


#endif //LIGHTDB_DROPFRAMES_H
