#ifndef LIGHTDB_SELECTFRAMESOPERATORS_H
#define LIGHTDB_SELECTFRAMESOPERATORS_H

#include "timer.h"
#include "PhysicalOperators.h"
#include "Stitcher.h"
#include <cassert>

namespace lightdb::physical {

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
                    metadataManager_(physical.source().filename())
        {}

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos())
                return {};

            GLOBAL_TIMER.startSection("HomomorphicSelectFrames");

            // Read the entire file.
            // Assume that we are getting CPUEncodedFrameData.
            std::vector<bytestring> encodedData(1);
            unsigned int width = 0;
            unsigned int height = 0;

            while (iterator() != iterator().eos()) {
                CPUEncodedFrameData frameData = iterator()++.downcast<CPUEncodedFrameData>();
                assert(frameData.codec() == Codec::hevc());
                auto data = frameData.value();
                encodedData[0].insert(encodedData[0].end(), data.begin(), data.end());

                if (!width) {
                    Configuration configuration = frameData.configuration();
                    width = configuration.width;
                    height = configuration.height;
                }
            }

            // Now that we have the entire byte string, insert/modify pic_output_flag.
            hevc::StitchContext context({1, 1}, {height, width});
            hevc::Stitcher stitcher(context, encodedData);

            auto framesToKeep = metadataManager_.framesForMetadata(physical().metadataSpecification());
            stitcher.addPicOutputFlagIfNecessaryKeepingFrames(framesToKeep);
            auto combinedNals = stitcher.combinedNalsForTile(0);

            auto returnVal = CPUEncodedFrameData(physical().source().codec(),
                                        physical().source().configuration(),
                                        physical().source().geometry(),
                                        combinedNals);
            GLOBAL_TIMER.endSection("HomomorphicSelectFrames");
            return {returnVal};
        }

    private:
        metadata::MetadataManager metadataManager_;
    };

    const catalog::Source source_;
    const MetadataSpecification metadataSpecification_;
};

} // namespace lightdb::physical


#endif //LIGHTDB_SELECTFRAMESOPERATORS_H
