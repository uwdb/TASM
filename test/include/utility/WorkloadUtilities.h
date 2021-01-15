#ifndef LIGHTDB_WORKLOADUTILITIES_H
#define LIGHTDB_WORKLOADUTILITIES_H

#include "Distribution.h"
#include "RegretAccumulator.h"
#include "Metadata.h"
#include "MetadataLightField.h"

namespace utility {
template<typename MetadataSpecificationType>
class WorkloadGenerator {
public:
    virtual std::shared_ptr<MetadataSpecificationType>
    getNextQuery(unsigned int queryNum, std::string *outQueryObject) = 0;

    virtual ~WorkloadGenerator() {};
};

class FrameGenerator {
public:
    virtual int nextFrame(std::default_random_engine &generator) = 0;
};

class UniformFrameGenerator : public FrameGenerator {
public:
    UniformFrameGenerator(const std::string &video, unsigned int duration, unsigned int totalFrames,
                          unsigned int framerate)
            : startingFrameDistribution_(0, totalFrames / framerate - duration - 1),
              framerate_(framerate) {}

    int nextFrame(std::default_random_engine &generator) override {
        auto startSecond = startingFrameDistribution_(generator);
        return startSecond * framerate_;
    }

private:
    std::uniform_int_distribution<int> startingFrameDistribution_;
    unsigned int framerate_;
};

template<typename MetadataSpecificationType>
class UniformRandomObjectWorkloadGenerator : public WorkloadGenerator<MetadataSpecificationType> {
public:
    UniformRandomObjectWorkloadGenerator(const std::string &video, const std::vector<std::vector<std::string>> &objects,
                                         unsigned int duration, std::default_random_engine *generator,
                                         std::unique_ptr<FrameGenerator> startingFrameDistribution,
                                         unsigned int framerate)
            : video_(video), objects_(objects), numberOfFramesInDuration_(duration * framerate),
              generator_(generator), startingFrameDistribution_(std::move(startingFrameDistribution)),
              probabilityGenerator_(42), probabilityDistribution_(0.0, 1.0) {
        assert(objects_.size() > 0 && objects_.size() <= 3);
    }

    std::shared_ptr<MetadataSpecificationType>
    getNextQuery(unsigned int queryNum, std::string *outQueryObject) override {
        auto prob = probabilityDistribution_(probabilityGenerator_);
        unsigned int index;

        if (objects_.size() == 1)
            index = 0;
        else if (objects_.size() == 2)
            index = prob < 0.5 ? 0 : 1;
        else if (objects_.size() == 3) {
            if (prob <= 0.33)
                index = 0;
            else if (prob >= 0.67)
                index = 2;
            else
                index = 1;
        } else {
            assert(false);
        }

        const std::vector<std::string> &objects = objects_[index];

        if (outQueryObject)
            *outQueryObject = lightdb::logical::TASMRegretAccumulator::combineStrings(objects);

        int firstFrame = startingFrameDistribution_->nextFrame(*generator_);
        int lastFrame = firstFrame + numberOfFramesInDuration_;

        auto metadataElement = lightdb::logical::TASMRegretAccumulator::MetadataElementForObjects(objects, firstFrame,
                                                                                                  lastFrame);

        return std::make_shared<MetadataSpecificationType>(
                "labels",
                metadataElement);
    }

private:
    std::string video_;
    std::vector<std::vector<std::string>> objects_;
    unsigned int numberOfFramesInDuration_;
    std::default_random_engine *generator_;
    std::unique_ptr<FrameGenerator> startingFrameDistribution_;

    std::default_random_engine probabilityGenerator_;
    std::uniform_real_distribution<float> probabilityDistribution_;
};

template
class WorkloadGenerator<lightdb::PixelsInFrameMetadataSpecification>;

} // namespace utility
#endif //LIGHTDB_WORKLOADUTILITIES_H
