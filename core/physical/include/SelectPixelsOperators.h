#ifndef LIGHTDB_SELECTPIXELSOPERATORS_H
#define LIGHTDB_SELECTPIXELSOPERATORS_H

#include "PhysicalOperators.h"
#include "Rectangle.h"
#include "SelectPixelsKernel.h"
#include "TileLayout.h"

namespace lightdb::physical {

class GPUNaiveSelectPixels : public PhysicalOperator, public GPUOperator {
public:
    explicit GPUNaiveSelectPixels(const LightFieldReference &logical,
            PhysicalOperatorReference &parent,
            unsigned int tileIndex,
            const tiles::TileLayout &tileLayout)
            : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUNaiveSelectPixels-init")),
            GPUOperator(parent),
            tileIndex_(tileIndex),
            tileLayout_(tileLayout)
    { }

    const logical::MetadataSubsetLightField &metadataSubsetLightField() const { return logical().downcast<logical::MetadataSubsetLightField>(); }
    unsigned int tileIndex() const { return tileIndex_; }
    const tiles::TileLayout &tileLayout() const { return tileLayout_; }

private:
    class Runtime : public runtime::GPUUnaryRuntime<GPUNaiveSelectPixels, GPUDecodedFrameData> {
    public:
        explicit Runtime(GPUNaiveSelectPixels &physical)
            : runtime::GPUUnaryRuntime<GPUNaiveSelectPixels, GPUDecodedFrameData>(physical),
                    transform_(video::GetSelectPixelsKernel(this->context())),
                    frameNumber_(0),
                    isSelectingPixelsOnUntiledVideo_(physical.tileLayout() == tiles::NoTilesLayout)
        { }

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos())
                return {};

            GLOBAL_TIMER.startSection("GPUNaiveSelectPixels");
            auto decoded = iterator()++;
            GPUDecodedFrameData output{decoded.configuration(), decoded.geometry()};

            auto tileRectangle = physical().tileLayout().rectangleForTile(physical().tileIndex());

            for (auto &frame: decoded.frames()) {
                long frameNumber = -1;
                frameNumber = frame->getFrameNumber(frameNumber) ? frameNumber : frameNumber_++;

                auto rectanglesForFrame = physical().metadataSubsetLightField().rectanglesForFrame(frameNumber);
                bool anyRectangleIntersectsTile = isSelectingPixelsOnUntiledVideo_
                        ? rectanglesForFrame.size()
                        : std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rect) {
                                return tileRectangle.intersects(rect);
                            });

//                if (!physical().metadataSubsetLightField().framesForMetadata().count(frameNumber)) {
                if (!anyRectangleIntersectsTile) {
                    continue;

//                    auto uv_offset = frame->width() * frame->height();
//                    auto uv_height = frame->height() / 2;
//                    auto cuda = frame->cuda();
//
//                    CHECK_EQ(cuMemsetD2D8(cuda->handle(),
//                            cuda->pitch(),
//                            0,
//                            cuda->width(),
//                            frame->height()), CUDA_SUCCESS);
//
//                    CHECK_EQ(cuMemsetD2D8(cuda->handle() + uv_offset,
//                                          cuda->pitch(),
//                                          128,
//                                          cuda->width(),
//                                          uv_height), CUDA_SUCCESS);
//e
//                    output.frames().emplace_back(frame);
                } else {
                    auto selected = transform_->nv12().select(this->lock(), frame->cuda(),
                                                             physical().metadataSubsetLightField().rectanglesForFrame(
                                                                     frameNumber), tileRectangle.x, tileRectangle.y);
                    output.frames().emplace_back(selected);
                }
            }

            GLOBAL_TIMER.endSection("GPUNaiveSelectPixels");
            return {output};
        }
    private:
        std::shared_ptr<video::GPUSelectPixels> transform_;
        int frameNumber_;
        bool isSelectingPixelsOnUntiledVideo_;
    };

private:
    unsigned int tileIndex_;
    tiles::TileLayout tileLayout_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_SELECTPIXELSOPERATORS_H
