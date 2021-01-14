#ifndef LIGHTDB_TASMOPERATORS_H
#define LIGHTDB_TASMOPERATORS_H

#include "PhysicalOperators.h"

#include "Gpac.h"
#include "MultipleEncoderManager.h"
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
                                Codec codec,
                                std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider,
                                bool splitByGOP = true)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUEncodeTilesToCPU-init", tileLocationProvider, splitByGOP)),
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
        explicit Runtime(GPUEncodeTilesToCPU &physical, std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider, bool splitByGOP)
            : runtime::GPUUnaryRuntime<GPUEncodeTilesToCPU, GPUDecodedFrameData>(physical),
                    tileLocationProvider_(tileLocationProvider),
                    splitByGOP_(splitByGOP),
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
            while ((!hitNewGOP || !splitByGOP_) && iterator() != iterator().eos()) {
                auto &decoded = *iterator();
                for (auto &frameIt = currentDataFramesIterator_; frameIt != decoded.frames().end(); frameIt++) {
                    long frameNumber;
                    assert((*frameIt)->getFrameNumber(frameNumber));
                    int tile = (*frameIt)->tileNumber();
                    assert(tile != -1);

                    if ((splitByGOP_ && gop(frameNumber) != currentGOP_) || tile != currentTile_) {
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
                    if (!splitByGOP_)
                        break;
                }
            }

            readEncoder(splitByGOP_ || iterator() == iterator().eos());
            auto encodedData = std::make_optional<CPUEncodedFrameData>(physical().codec(), currentConfiguration_, geometry_, coalescedEncodedData());
            encodedData->setFirstFrameIndexAndNumberOfFrames(firstFrame_, numberOfFrames);
            encodedData->setTileNumber(currentTile_);
            return encodedData;
        }

    private:
        int gop(long frameNumber) {
            return frameNumber / gopLength_;
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

            if (!encoderManager_.hasEncoder(encoderId_))
                encoderManager_.createEncoderWithConfiguration(encoderId_, (*currentDataFramesIterator_)->width(), (*currentDataFramesIterator_)->height(), std::nullopt);
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

        std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider_;
        bool splitByGOP_;
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

class StoreEncodedTiles : public PhysicalOperator {
public:
    explicit StoreEncodedTiles(const LightFieldReference &logical,
                                PhysicalOperatorReference &parent,
                                std::filesystem::path basePath,
                                const Codec &codec,
                                std::shared_ptr<tiles::ModifiedTileTracker> modifiedTileTracker)
        : PhysicalOperator(logical, {parent}, DeviceType::CPU, runtime::make<Runtime>(*this, "StoreEncodedTiles-init", basePath, modifiedTileTracker)),
        codec_(codec)
    { }

    const Codec &codec() const { return codec_; }

private:
    class Runtime: public runtime::UnaryRuntime<StoreEncodedTiles, CPUEncodedFrameData> {
    public:
        Runtime(StoreEncodedTiles &physical, std::filesystem::path basePath, std::shared_ptr<tiles::ModifiedTileTracker> modifiedTileTracker)
            : runtime::UnaryRuntime<StoreEncodedTiles, CPUEncodedFrameData>(physical),
                    basePath_(basePath),
                    modifiedTileTracker_(modifiedTileTracker)
        {
            // Remove previous temporary contents.
            std::filesystem::remove_all(catalog::TmpTileFiles::temporaryDirectory(basePath_));
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos())
                return std::nullopt;

            auto data = iterator()++;
            int firstFrame;
            long numberOfFrames;
            int tileNumber;
            assert(data.getFirstFrameIndexIfSet(firstFrame));
            assert(data.getNumberOfFramesIfSet(numberOfFrames));
            assert(data.getTileNumberIfSet(tileNumber));
            int lastFrame = firstFrame + numberOfFrames - 1;

            auto outputDir = setUpDirectory(firstFrame, lastFrame);
            auto rawVideoPath = writeRawVideo(outputDir, tileNumber, data.value());
//            auto muxedVideo = muxVideo(rawVideoPath);
            if (modifiedTileTracker_) {
                modifiedTileTracker_->modifiedTileInFrames(tileNumber, firstFrame, lastFrame, outputDir);
            }
            return CPUEncodedFrameData(data.codec(), data.configuration(), data.geometry(), std::make_unique<bytestring>());
        }

    private:
        std::filesystem::path setUpDirectory(int firstFrame, int lastFrame) {
            auto framesPath = catalog::TmpTileFiles::directoryForTilesInFrames(basePath_, firstFrame, lastFrame);
            std::filesystem::create_directories(framesPath);
            return framesPath;
        }

        std::filesystem::path writeRawVideo(const std::filesystem::path &outputDir, int tileNumber, const bytestring &value) {
            auto rawTilePath = catalog::TileFiles::temporaryTileFilename(outputDir, tileNumber);
            std::ofstream hevc(rawTilePath);
            hevc.write(value.data(), value.size());
            return rawTilePath;
        }

        std::filesystem::path muxVideo(const std::filesystem::path &rawVideoPath) {
            auto muxedFile = rawVideoPath;
            muxedFile.replace_extension(catalog::TileFiles::muxedFilenameExtension());
            video::gpac::mux_media(rawVideoPath, muxedFile, physical().codec());
            return muxedFile;
        }

        std::filesystem::path basePath_;
        std::shared_ptr<tiles::ModifiedTileTracker> modifiedTileTracker_;
    };
    const Codec codec_;
};

class StitchOperator : public PhysicalOperator {
public:
    explicit StitchOperator(const LightFieldReference &logical,
                            PhysicalOperatorReference &parent,
                            std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider,
                            unsigned int gopLength,
                            unsigned int firstFrame,
                            unsigned int lastFrameExclusive)
        : PhysicalOperator(logical, {parent}, DeviceType::CPU, runtime::make<Runtime>(*this, "StitchOperator-init", tileLocationProvider, gopLength, firstFrame, lastFrameExclusive))
    { }

private:
    class Runtime : public runtime::UnaryRuntime<StitchOperator, CPUEncodedFrameData> {
        public:
            Runtime(StitchOperator &physical,
                    std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider,
                    unsigned int gopLength,
                    unsigned int firstFrame,
                    unsigned int lastFrame)
                : runtime::UnaryRuntime<StitchOperator, CPUEncodedFrameData>(physical),
                        tileLocationProvider_(tileLocationProvider),
                        gopLength_(gopLength),
                        firstGOP_(gop(firstFrame)),
                        lastGOP_(gop(lastFrame)),
                        currentGOP_(firstGOP_),
                        ppsId_(1),
                        sentSEIForOneTile_(false),
                        numberOfStitchedGOPs_(0),
                        timer_(new Timer()),
                        partTimer_(new Timer())
            { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            // Make sure everything is processed/stored.
            preProcessEverything();

            if (currentGOP_ >= lastGOP_) {
                std::cout << "**Number of stitched GOPs: " << numberOfStitchedGOPs_ << std::endl;
                timer_->printAllTimes();
                partTimer_->printAllTimes();
                return std::nullopt;
            }

            timer_->startSection("stitch");
            auto stitched = stitchGOP(currentGOP_);
            timer_->endSection("stitch");
            if (!sentSEIForOneTile_) {
                ++currentGOP_;
                ++numberOfStitchedGOPs_;
            }
            auto &base = referenceData_->downcast<CPUEncodedFrameData>();
            return std::make_optional<CPUEncodedFrameData>(base.codec(), base.configuration(), base.geometry(), stitched);
        }

    private:
        void preProcessEverything() {
            while(!all_parent_eos()) {
                for (auto it = iterators().begin(); it != iterators().end(); ++it) {
                    if (*it != it->eos()) {
                        if (!referenceData_)
                            referenceData_ = std::shared_ptr<MaterializedLightField>(**it);
                        ++(*it);
                    }
                }
            }
        }

        int gop(unsigned int frame) { return frame / gopLength_; }
        std::shared_ptr<bytestring> stitchGOP(int gop);
        std::shared_ptr<bytestring> getSEI();

        std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider_;
        unsigned int gopLength_;
        unsigned int firstGOP_;
        unsigned int lastGOP_;
        unsigned int currentGOP_;
        unsigned int ppsId_;
        std::shared_ptr<MaterializedLightField> referenceData_;
        static constexpr auto seiPath_ = "/home/maureen/lightdb-wip/HomomorphicStitching/scripts/sei.txt";
        std::shared_ptr<bytestring> sei_;
        bool sentSEIForOneTile_;
        unsigned int numberOfStitchedGOPs_;
        std::unique_ptr<Timer> timer_;
        std::unique_ptr<Timer> partTimer_;
    };
};

class GPUCreateBlackTile : public PhysicalOperator, public GPUOperator {
public:
    // For now, assume numFrames == framerate.
    explicit GPUCreateBlackTile(const LightFieldReference &logical,
                                const execution::GPU &gpu,
                                Codec codec,
                                unsigned int width,
                                unsigned int height,
                                unsigned int numFrames)
        : PhysicalOperator(logical, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUCreateBlackTile-init", codec, width, height, numFrames)),
        GPUOperator(gpu)
    {
        if (!codec.nvidiaId().has_value())
            throw GpuRuntimeError("Requested codec does not have an Nvidia encode id");
    }

private:
    class Runtime : public runtime::GPURuntime<GPUCreateBlackTile> {
    public:
        explicit Runtime(GPUCreateBlackTile &physical,
                Codec codec,
                unsigned int width,
                unsigned int height,
                unsigned int numFrames):
            runtime::GPURuntime<GPUCreateBlackTile>(physical),
            codec_(codec),
            width_(width),
            height_(height),
            numFrames_(numFrames),
            done_(false)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (done_)
                return std::nullopt;

            done_ = true;
            return makeTiles();
        }

    private:
        std::optional<physical::MaterializedLightFieldReference> makeTiles();
        std::pair<CUdeviceptr, size_t> makeFrame();

        const Codec codec_;
        const unsigned int width_;
        const unsigned int height_;
        const unsigned int numFrames_;
        bool done_;
    };
};

class GPUSaveToArray : public PhysicalOperator, public GPUOperator {
public:
    explicit GPUSaveToArray(const LightFieldReference &logical,
            PhysicalOperatorReference &parent,
            std::filesystem::path basePath)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "GPUSaveToArray-init", basePath)),
        GPUOperator(parent)
    { }

private:
    class Runtime : public runtime::GPUUnaryRuntime<GPUSaveToArray, GPUDecodedFrameData> {
    public:
        static constexpr size_t kDefaultGopSize = 30;

        explicit Runtime(GPUSaveToArray &physical, std::filesystem::path basePath)
                : runtime::GPUUnaryRuntime<GPUSaveToArray, GPUDecodedFrameData>(physical),
                    basePath_(basePath),
                  gopLength_(gop(configuration().framerate)),
                  currentGOP_(-1),
                  currentTile_(-1),
                  firstFrame_(-1),
                  width_(0),
                  height_(0),
                  yuvHandle_(0),
                  bgrHandle_(0),
                  packedFloatHandle_(0),
                  planarHandle_(0)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            std::unique_ptr<std::ofstream> fout;
            int maxFramesPerGOP = 3;
            int count = 0;
            while (iterator() != iterator().eos()) {
                auto &decoded = *iterator();
                for (auto &frame : decoded.frames()) {
                    long frameNumber;
                    assert(frame->getFrameNumber(frameNumber));
                    if (gop(frameNumber) != currentGOP_ || frame->tileNumber() != currentTile_) {
                        if (fout) fout->close();
                        currentGOP_ = gop(frameNumber);
                        currentTile_ = frame->tileNumber();
                        fout = std::make_unique<std::ofstream>(outputFile(currentGOP_, currentTile_, frame->width(), frame->height()),  std::ios::out | std::ios::binary);
                        count = 0;
                    }

                    if (count >= maxFramesPerGOP)
                        continue;
                    processFrame(frame);
                    auto pixels = pixelData();
                    fout->write(reinterpret_cast<const char*>(pixels->data()), pixels->size());
                    ++count;
                }
                ++iterator();
            }
            return {};
        }


    private:
        int gop(long frameNumber) {
            return frameNumber / gopLength_;
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

        std::filesystem::path outputFile(unsigned int gop, unsigned int tile, unsigned int width, unsigned int height) {
            auto out = basePath_ / ("gop_" + std::to_string(gop) + "_tile_" + std::to_string(tile) + "_" + std::to_string(width) + "x" + std::to_string(height) + ".npy");
            std::cout << "Array output path: " << out << std::endl;
            return out;
        }

        void allocate(unsigned int width, unsigned int height);
        void deallocate();
        void processFrame(GPUFrameReference &frame);
        std::unique_ptr<std::vector<unsigned char>> pixelData();

        std::filesystem::path basePath_;
        unsigned int gopLength_;
        int currentGOP_;
        int currentTile_;
        int firstFrame_;

        unsigned int width_;
        unsigned int height_;
        const unsigned int channels_ = 3;
        CUdeviceptr yuvHandle_;
        size_t yuvPitch_;
        CUdeviceptr bgrHandle_;
        size_t bgrPitch_;
        CUdeviceptr packedFloatHandle_;
        size_t packedFloatPitch_;
        CUdeviceptr planarHandle_;
    };


};

} // namespace lightdb::physical

#endif //LIGHTDB_TASMOPERATORS_H
