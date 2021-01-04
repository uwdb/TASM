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

class GPUEncodeTilesToCPU : public PhysicalOperator, public GPUOperator {
public:
    static constexpr size_t kDefaultGopSize = 30;

    explicit GPUEncodeTilesToCPU(const LightFieldReference &logical,
                                PhysicalOperatorReference &parent,
                                Codec codec)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUEncodeTilesToCPU-init")),
        GPUOperator(parent),
        codec_(codec) {
        if (!codec.nvidiaId().has_value())
            throw GpuRuntimeError("Requested codec does not have an Nvidia encode id");
    }

    const Codec &codec() const { return codec_; }

private:
    class Runtime: public runtime::GPUUnaryRuntime<GPUEncodeTilesToCPU, GPUDecodedFrameData> {
    public:
        // Do we need to make sure the base configuration is big enough for all tiles?
        // Maybe it's not an issue right now because we decode in descending tile size.
        explicit Runtime(GPUEncodeTilesToCPU &physical)
            : runtime::GPUUnaryRuntime<GPUEncodeTilesToCPU, GPUDecodedFrameData>(physical),
                    numberOfEncodedFrames_(0),
                    gopLength_(gop(configuration().framerate)),
                    geometry_(geometry()),
                    encoderManager_(EncodeConfiguration(configuration(), physical.codec().nvidiaId().value(), gopLength_), context(), lock()),
                    currentGOP_(-1),
                    currentTile_(-1),
                    firstFrame_(-1),
                    currentDataFramesIteratorIsValid_(false),
                    encodedDataLength_(0)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos()) {
                std::cout << "** Encoded " << numberOfEncodedFrames_ << " frames\n";
                return std::nullopt;
            }

            setUpFrameIterator();
            updateGOPTileAndEncoder();
            bool hitNewGOP = false;
            int numberOfFrames = 0;
            while (!hitNewGOP && iterator() != iterator().eos()) {
                auto &decoded = *iterator();
                for (auto &frameIt = currentDataFramesIterator_; frameIt != decoded.frames().end(); frameIt++) {
                    long frameNumber;
                    assert((*frameIt)->getFrameNumber(frameNumber));
                    int tile = (*frameIt)->tileNumber();
                    assert(tile != -1);

                    if (gop(frameNumber) != currentGOP_ || tile != currentTile_) {
                        hitNewGOP = true;
                        break;
                    }

                    encoderManager_.encodeFrameForIdentifier(encoderId_, **frameIt, 0, 0, false);
                    ++numberOfFrames;
                    ++numberOfEncodedFrames_;
                }

                readEncoder(false);
                if (noMoreFramesInCurrentData()) {
                    ++iterator();
                    currentDataFramesIteratorIsValid_ = false;
                    setUpFrameIterator();
                }
            }

            readEncoder(true);
            auto encodedData = std::make_optional<CPUEncodedFrameData>(physical().codec(), currentConfiguration_, geometry_, coalescedEncodedData());
            encodedData->setFirstFrameIndexAndNumberOfFrames(firstFrame_, numberOfFrames);
            encodedData->setTileNumber(currentTile_);
            return encodedData;
        }

    private:
        int gop(long frameNumber) {
            return gopLength_ / frameNumber;
        }

        unsigned int gop(const Configuration::FrameRate &framerate) const {
            auto option = logical().is<OptionContainer<>>()
                          ? logical().downcast<OptionContainer<>>().get_option(EncodeOptions::GOPSize)
                          : std::nullopt;

            if(option.has_value() && option.value().type() != typeid(unsigned int))
                throw InvalidArgumentError("Invalid GOP option specified", EncodeOptions::GOPSize);
            else {
                return std::any_cast<unsigned int>(option.value_or(
                        std::make_any<unsigned int>(framerate.denominator() == 1 ? framerate.numerator() : kDefaultGopSize)));
            }
        }

        void setUpFrameIterator() {
            if (currentDataFramesIteratorIsValid_)
                return;

            while (iterator() != iterator().eos()) {
                auto &decoded = *iterator();
                if (decoded.frames().begin() != decoded.frames().end()) {
                    currentDataFramesIterator_ = decoded.frames().begin();
                    currentDataFramesIteratorIsValid_ = true;
                    currentConfiguration_ = decoded.configuration();
                    return;
                }

                ++iterator();
            }
        }

        void updateGOPTileAndEncoder() {
            // This could happen if there are no more frames.
            if (!currentDataFramesIteratorIsValid_)
                return;

            long frameNumber;
            assert((*currentDataFramesIterator_)->getFrameNumber(frameNumber));
            currentGOP_ = gop(frameNumber);
            firstFrame_ = int(frameNumber);
            currentTile_ = (*currentDataFramesIterator_)->tileNumber();
            assert(currentTile_ != -1);

            encoderManager_.createEncoderWithConfiguration(encoderId_, (*currentDataFramesIterator_)->width(), (*currentDataFramesIterator_)->height());
        }

        bool noMoreFramesInCurrentData() {
            return currentDataFramesIterator_ == (*iterator()).frames().end();
        }

        void readEncoder(bool shouldFlush = false) {
            auto encodedData = shouldFlush ? encoderManager_.flushEncoderForIdentifier(encoderId_) : encoderManager_.getEncodedFramesForIdentifier(encoderId_);
            if (!encodedData->empty()) {
                encodedDataForGOPAndTile_.push_back(std::move(encodedData));
                encodedDataLength_ += encodedDataForGOPAndTile_.back()->size();
            }
        }

        std::unique_ptr<bytestring> coalescedEncodedData() {
            std::unique_ptr<bytestring> coalesced(new bytestring());
            coalesced->reserve(encodedDataLength_);
            for (auto it = encodedDataForGOPAndTile_.begin(); it != encodedDataForGOPAndTile_.end(); ++it) {
                coalesced->insert(coalesced->end(), (*it)->begin(), (*it)->end());
            }

            encodedDataForGOPAndTile_.clear();
            encodedDataLength_ = 0;
            return coalesced;
        }

        unsigned long numberOfEncodedFrames_;
        unsigned int gopLength_;
        GeometryReference geometry_;
        MultipleEncoderManager encoderManager_;
        int currentGOP_;
        int currentTile_;
        int firstFrame_;
        Configuration currentConfiguration_;
        std::vector<GPUFrameReference>::const_iterator currentDataFramesIterator_;
        bool currentDataFramesIteratorIsValid_;
        const unsigned int encoderId_ = 0u;
        std::list<std::unique_ptr<bytestring>> encodedDataForGOPAndTile_;
        size_t encodedDataLength_;
    };
    const Codec codec_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_TASMOPERATORS_H
