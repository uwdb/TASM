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
                                PhysicalOperatorReference &parent)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "NaiveSelectFrames-init")),
        GPUOperator(parent),
        metadataSpecification_(logical.downcast<logical::MetadataSubsetLightField>().metadataSpecification()),
        source_(logical.downcast<logical::MetadataSubsetLightField>().source()),
        metadataManager_(source_.filename())
    {}

    const MetadataSpecification &metadataSpecification() const { return metadataSpecification_; }
    const catalog::Source &source() const { return source_; }
    std::unordered_set<int> framesToKeep() const { return metadataManager_.framesForMetadata(metadataSpecification()); }

private:
    class Runtime: public runtime::UnaryRuntime<NaiveSelectFrames, GPUDecodedFrameData> {
    public:
        explicit Runtime(NaiveSelectFrames &physical)
            : runtime::UnaryRuntime<NaiveSelectFrames, GPUDecodedFrameData>(physical),
                    frameNumber_(0)
        {}

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos())
                return {};

            // This intentionally does nothing because this physical operator never actually ends up in a physical plan.
            return {};
        }
    private:
        unsigned long frameNumber_;
    };

    const MetadataSpecification metadataSpecification_;
    const catalog::Source source_;
    const metadata::MetadataManager metadataManager_;
};

class HomomorphicSelectFrames: public PhysicalOperator {
public:
    explicit HomomorphicSelectFrames(const LightFieldReference &logical,
                                     const PhysicalOperatorReference &parent,
                                     const catalog::Source &source)
            : PhysicalOperator(logical, {parent}, DeviceType::CPU, runtime::make<Runtime>(*this, "HomomorphicSelectFrames-init")),
            source_(source),
            metadataSpecification_(logical.downcast<logical::MetadataSubsetLightField>().metadataSpecification())
    {}


    const catalog::Source &source() const { return source_; }
    const MetadataSpecification &metadataSpecification() const { return metadataSpecification_; }

private:
    class Runtime: public runtime::UnaryRuntime<HomomorphicSelectFrames, CPUEncodedFrameData> {
    public:
        explicit Runtime(HomomorphicSelectFrames &physical)
            : runtime::UnaryRuntime<HomomorphicSelectFrames, CPUEncodedFrameData>(physical),
                    metadataManager_(physical.source().filename()),
                    framesToKeep_(metadataManager_.framesForMetadata(physical.metadataSpecification())),
                    picOutputFlagAdder_(framesToKeep_),
                    doNotHaveToAddPicOutputFlag_(physical.source().mp4Reader().allFrameSequencesBeginWithKeyframe(
                            metadataManager_.orderedFramesForMetadata(physical.metadataSpecification())))
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
        metadata::MetadataManager metadataManager_;
        std::unordered_set<int> framesToKeep_;
        hevc::PicOutputFlagAdder picOutputFlagAdder_;
        bool doNotHaveToAddPicOutputFlag_;
    };

    const catalog::Source source_;
    const MetadataSpecification metadataSpecification_;
};

} // namespace lightdb::physical


#endif //LIGHTDB_SELECTFRAMESOPERATORS_H
