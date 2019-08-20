#ifndef LIGHTDB_SELECTPIXELSOPERATORS_H
#define LIGHTDB_SELECTPIXELSOPERATORS_H

#include "PhysicalOperators.h"
#include "Rectangle.h"
#include "SelectPixelsKernel.h"

namespace lightdb::physical {

class GPUNaiveSelectPixels : public PhysicalOperator, public GPUOperator {
public:
    explicit GPUNaiveSelectPixels(const LightFieldReference &logical,
            PhysicalOperatorReference &parent)
            : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUNaiveSelectPixels-init")),
            GPUOperator(parent)
    { }

    const logical::MetadataSubsetLightField &metadataSubsetLightField() const { return logical().downcast<logical::MetadataSubsetLightField>(); }

private:
    class Runtime : public runtime::GPUUnaryRuntime<GPUNaiveSelectPixels, GPUDecodedFrameData> {
    public:
        explicit Runtime(GPUNaiveSelectPixels &physical)
            : runtime::GPUUnaryRuntime<GPUNaiveSelectPixels, GPUDecodedFrameData>(physical),
                    transform_(this->context()),
                    frameNumber_(0)
        { }

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos())
                return {};

            GLOBAL_TIMER.startSection("GPUNaiveSelectPixels");
            auto decoded = iterator()++;
            GPUDecodedFrameData output{decoded.configuration(), decoded.geometry()};

            for (auto &frame: decoded.frames()) {
                auto frameNumber = frameNumber_++;
                if (!physical().metadataSubsetLightField().framesForMetadata().count(frameNumber))
                    continue;

                auto selected = transform_.nv12().select(this->lock(), frame->cuda(), physical().metadataSubsetLightField().rectanglesForFrame(frameNumber));
                output.frames().emplace_back(selected);
            }

            GLOBAL_TIMER.endSection("GPUNaiveSelectPixels");
            return {output};
        }
    private:
        video::GPUSelectPixels transform_;
        int frameNumber_;
    };
};

} // namespace lightdb::physical

#endif //LIGHTDB_SELECTPIXELSOPERATORS_H
