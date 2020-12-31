#ifndef LIGHTDB_TASMOPERATORS_H
#define LIGHTDB_TASMOPERATORS_H

#include "PhysicalOperators.h"

#include "Tasm.h"

namespace lightdb::physical {

template<typename Transform>
class GPUMetadataTransform : public PhysicalOperator, public GPUOperator {
public:
    explicit GPUMetadataTransform(const LightFieldReference &logical,
            PhysicalOperatorReference &parent,
            MetadataSpecification specification,
            std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider)
                : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUMetadataTransform-init", tileLocationProvider)),
                GPUOperator(parent),
                specification_(specification)
    {
        assert(logical->tasm());
    }

    MetadataSpecification specification() const { return specification_; }

    GPUMetadataTransform(const GPUMetadataTransform&) = delete;

private:
    class Runtime: public runtime::GPUUnaryRuntime<GPUMetadataTransform, GPUDecodedFrameData> {
    public:
        explicit Runtime(GPUMetadataTransform &physical, std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider)
            : runtime::GPUUnaryRuntime<GPUMetadataTransform, GPUDecodedFrameData>(physical),
                    transform_(this->context()),
                    tasm_(physical.logical()->tasm()),
                    tileLocationProvider_(tileLocationProvider)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (this->iterator() != this->iterator().eos()) {
                auto data = this->iterator()++;
                GPUDecodedFrameData output{data.configuration(), data.geometry()};

                auto specification = this->physical().specification();
                for (auto &frame : data.frames()) {
                    long frameNumber;
                    assert(frame->getFrameNumber(frameNumber));
                    int tileNumber = frame->tileNumber();
                    assert(tileNumber >= 0);
                    auto tileRectangle = tileLocationProvider_->tileLayoutForFrame(frameNumber).rectangleForTile(tileNumber);

                    output.frames().emplace_back(transform_.nv12().draw(this->lock(),
                            frame->cuda(),
                            *tasm_->getMetadata("", specification, frameNumber),
                            tileRectangle.x, tileRectangle.y));
                }
                return {output};
            } else {
                return {};
            }
        }

    private:
        Transform transform_;
        std::shared_ptr<Tasm> tasm_;
        std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider_;
    };
    MetadataSpecification specification_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_TASMOPERATORS_H
