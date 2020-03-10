#ifndef LIGHTDB_SELECTFRAMESOPERATORS_H
#define LIGHTDB_SELECTFRAMESOPERATORS_H

#include "timer.h"
#include "PhysicalOperators.h"
#include "Stitcher.h"
#include "MP4Reader.h"
#include <cassert>
#include <iostream>

namespace lightdb::physical {

class NaiveSelectFrames: public PhysicalOperator, GPUOperator {
public:
    explicit NaiveSelectFrames(const LightFieldReference &logical,
                                PhysicalOperatorReference &parent,
                                const std::unordered_set<int> &framesToKeep)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "NaiveSelectFrames-init")),
        GPUOperator(parent),
        framesToKeep_(framesToKeep)
    {}

    const std::unordered_set<int> &framesToKeep() const { return framesToKeep_; }

private:
    class Runtime: public runtime::UnaryRuntime<NaiveSelectFrames, GPUDecodedFrameData> {
    public:
        explicit Runtime(NaiveSelectFrames &physical)
            : runtime::UnaryRuntime<NaiveSelectFrames, GPUDecodedFrameData>(physical),
                    frameNumber_(0),
                    numberOfKeptFrames_(0)
        {}

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos()) {
                std::cout << "\nANALYSIS: num-kept-frames " << numberOfKeptFrames_ << std::endl;
                return {};
            }

            auto decodedReference = iterator()++;
            auto decoded = decodedReference.downcast<GPUDecodedFrameData>();
            auto configuration = decoded.configuration();
            auto geometry = decoded.geometry();
            physical::GPUDecodedFrameData output{configuration, geometry};

            for (const auto &frame : decoded.frames()) {
                int frameNumber = -1;
                frameNumber = frame->getFrameNumber(frameNumber) ? frameNumber : frameNumber_++;
                if (physical().framesToKeep().count(frameNumber)) {
                    output.frames().emplace_back(frame);
                    ++numberOfKeptFrames_;
                }
            }
            return {output};
        }
    private:
        unsigned long frameNumber_;
        unsigned int numberOfKeptFrames_;
    };

    const std::unordered_set<int> framesToKeep_;
};

class HomomorphicSelectFrames: public PhysicalOperator {
public:
    explicit HomomorphicSelectFrames(const LightFieldReference &logical,
                                     const PhysicalOperatorReference &parent,
                                     const catalog::Source &source)
            : PhysicalOperator(logical, {parent}, DeviceType::CPU, runtime::make<Runtime>(*this, "HomomorphicSelectFrames-init")),
            source_(source)
    {}


    const catalog::Source &source() const { return source_; }
    const logical::MetadataSubsetLightField &metadataSubsetLightField() const { return logical().downcast<logical::MetadataSubsetLightField>(); }

private:
    class Runtime: public runtime::UnaryRuntime<HomomorphicSelectFrames, CPUEncodedFrameData> {
    public:
        explicit Runtime(HomomorphicSelectFrames &physical)
            : runtime::UnaryRuntime<HomomorphicSelectFrames, CPUEncodedFrameData>(physical),
                    framesToKeep_(physical.metadataSubsetLightField().framesForMetadata()),
                    picOutputFlagAdder_(framesToKeep_),
                    doNotHaveToAddPicOutputFlag_(physical.source().mp4Reader().allFrameSequencesBeginWithKeyframe(
                            physical.metadataSubsetLightField().orderedFramesForMetadata()))
        {
            if (doNotHaveToAddPicOutputFlag_)
                std::cout << "Skipping adding pic_output_flag\n";
            else
                std::cout << "Adding pic_output_flag\n";
        }

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos())
                return {};

            GLOBAL_TIMER.startSection("HomomorphicSelectFrames");

            CPUEncodedFrameData frameData = iterator()++.downcast<CPUEncodedFrameData>();
            assert(frameData.codec() == Codec::hevc());

            if (doNotHaveToAddPicOutputFlag_) {
                GLOBAL_TIMER.endSection("HomomorphicSelectFrames");
                return frameData;
            }

            // Assume frameData contains a complete GOP.

            // Now analyze the nals in the bitstream. Only consider the last nal to be complete if iterator() == eos().
            // Otherwise, move the last nal to bitsLeftOver.
            Timer timer;
            timer.startSection("AddPicOutputFlag");
            int firstFrameIndex;
            assert(frameData.getFirstFrameIndexIfSet(firstFrameIndex));
            auto gopData = frameData.value();
            // TODO: If all of the frames we are keeping from the GOP are sequential, then there is no need to add the flag.
            // If some GOPs are sequential and others are not, we will have to make a new PPS and update the slices to reference
            // the correct PPS.
            picOutputFlagAdder_.addPicOutputFlagToGOP(gopData, firstFrameIndex, framesToKeep_);
            timer.endSection("AddPicOutputFlag");
//            timer.printAllTimes();

            auto returnVal = CPUEncodedFrameData(physical().source().codec(),
                                        physical().source().configuration(),
                                        physical().source().geometry(),
                                        gopData);
            GLOBAL_TIMER.endSection("HomomorphicSelectFrames");
            return returnVal;
        }

    private:
        std::unordered_set<int> framesToKeep_;
        hevc::PicOutputFlagAdder picOutputFlagAdder_;
        bool doNotHaveToAddPicOutputFlag_;
    };

    const catalog::Source source_;
};

} // namespace lightdb::physical


#endif //LIGHTDB_SELECTFRAMESOPERATORS_H
