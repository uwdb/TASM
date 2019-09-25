#ifndef LIGHTDB_TILEOPERATORS_H
#define LIGHTDB_TILEOPERATORS_H

#include "PhysicalOperators.h"
#include "Rectangle.h"
#include "MultipleEncoderManager.h"

#include "ipp.h"

namespace lightdb::physical {

class MergeTilePixels: public PhysicalOperator, public GPUOperator {
public:
    explicit MergeTilePixels(const LightFieldReference &logical,
            std::vector<PhysicalOperatorReference> &parents,
            std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider)
        : PhysicalOperator(logical, parents, DeviceType::GPU, runtime::make<Runtime>(*this, "MergeTilePixels-init")),
        GPUOperator(parents.front()),
        metadataManager_(getMetadataManager()),
        tileLayout_(tiles::EmptyTileLayout),
        parentIndexToTileNumber_{},
        tileLocationProvider_(tileLocationProvider)
    { }

    explicit MergeTilePixels(const LightFieldReference &logical,
                             PhysicalOperatorReference parent,
                             std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider)
            : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "MergeTilePixels-init")),
              GPUOperator(parent),
              metadataManager_(getMetadataManager()),
              tileLayout_(tiles::EmptyTileLayout),
              parentIndexToTileNumber_{},
              tileLocationProvider_(tileLocationProvider)
    { }

    explicit MergeTilePixels(const LightFieldReference &logical,
            std::vector<PhysicalOperatorReference> &parents,
            const tiles::TileLayout &tileLayout,
            std::unordered_map<int, int> parentIndexToTileNumber)
        : PhysicalOperator(logical, parents, DeviceType::GPU, runtime::make<Runtime>(*this, "MergeTilePixels-init")),
        GPUOperator(parents.front()),
        metadataManager_(getMetadataManager()),
        tileLayout_(tileLayout),
        parentIndexToTileNumber_(std::move(parentIndexToTileNumber))
    { }

    const std::vector<Rectangle> &rectanglesForFrame(unsigned int frameNumber) const {
        return metadataManager_.rectanglesForFrame(frameNumber);
    }

    const logical::MetadataSubsetLightField &metadataSubsetLightField() const { return logical().downcast<logical::MetadataSubsetLightField>(); }
    const tiles::TileLayout &tileLayoutForFrame(unsigned int frameNumber) const {
        if (tileLocationProvider_)
            return tileLocationProvider_->tileLayoutForFrame(frameNumber);
        else
            return tileLayout_;
    }

    const tiles::TileLayout &tileLayout() const { return tileLayout_; }

    int tileNumberForParentIndex(int parentIndex) const {
        if (tileLocationProvider_)
            return parentIndex;
        else if (tileLayout_ == tiles::NoTilesLayout)
            return parentIndex;
        else
            return parentIndexToTileNumber_.at(parentIndex);
    }

private:
    class Runtime: public runtime::GPURuntime<MergeTilePixels> {
    public:
        explicit Runtime(MergeTilePixels &physical)
            : runtime::GPURuntime<MergeTilePixels>(physical),
                    maxProcessedFrameNumbers_(physical.parents().size(), -1),
                    numberOfRectanglesProcessed_(0),
                    inNoTilesCase_(false),
                    geometry_(GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples()))
        {
            setConfiguration();

            if (physical.tileLayout() == tiles::NoTilesLayout) {
                inNoTilesCase_ = true;
                tileLayoutForNoTilesCase_ = tiles::TileLayout(1, 1, {configuration_.width}, {configuration_.height});
            }
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (all_parent_eos()) {
                assert(rectangleToPixels_.empty());
                std::cout << "Number of processed rectangles: " << numberOfRectanglesProcessed_ << std::endl;
                return {};
            }

            GLOBAL_TIMER.startSection("MergeTilePixels");
            if (!all_parent_eos()) {
                for (auto index = 0u; index < iterators().size(); index++) {
                    if (iterators()[index] != iterators()[index].eos()) {
//                        auto tileNumber = physical().tileNumberForParentIndex(index);
                        auto decoded = (iterators()[index]++).downcast<GPUDecodedFrameData>();

                        for (auto &frame : decoded.frames()) {
                            // Get rectangles for frame.
                            int frameNumber = -1;
                            assert(frame->getFrameNumber(frameNumber));
                            int tileNumber = inNoTilesCase_ ? 0 : frame.downcast<DecodedFrame>().tileNumber();

                            auto &tileLayout = tileLayoutForFrame(frameNumber);
                            auto tileRectangle = tileLayout.rectangleForTile(tileNumber);

                            const auto &rectanglesForFrame = physical().rectanglesForFrame(frameNumber);
                            bool anyRectangleIntersectsTile = std::any_of(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](auto &rect) {
                                return tileRectangle.intersects(rect);
                            });

                            if (!anyRectangleIntersectsTile)
                                continue;

                            // For each rectangle that the tile overlaps, copy the tile's pixels to the correct
                            // location in the rectangle's frame.
                            std::for_each(rectanglesForFrame.begin(), rectanglesForFrame.end(), [&](const Rectangle &rectangle) {
                                if (!tileRectangle.intersects(rectangle))
                                    return;

                                copyTilePixelsFromFrameToRectangleFrame(tileRectangle, frame, rectangle, tileLayout);
                            });
                        }

//                        if (iterators()[index] == iterators()[index].eos())
//                            updateMaxProcessedFrameNumber(INT32_MAX, index);
                    } else {
//                        updateMaxProcessedFrameNumber(INT32_MAX, index);
                    }
                }
            }

            // Return "frames" for rectangles that are on frames that every parent has processed.
            // Find rectangles that have a frame id <= the maximum processed frame, and put those into
            // the "frames" array of a GPUDecodedFramesObject thing.
            auto pixelsAsFrames = pixelsForProcessedRectangles();
//            auto returnVal = GPUPixelData(pixelsAsFrames);
            auto returnVal = GPUDecodedFrameData{configuration_, geometry_, pixelsAsFrames};
            GLOBAL_TIMER.endSection("MergeTilePixels");
            return returnVal;
        }

    private:
        const tiles::TileLayout &tileLayoutForFrame(unsigned int frame) const {
            return inNoTilesCase_ ? tileLayoutForNoTilesCase_ : physical().tileLayoutForFrame(frame);
        }

        void setConfiguration() {
            // Find an interator that is not eos.
            // If they are all eos, then no configuration.
            for (auto & iterator : iterators()) {
                if (iterator == iterator.eos())
                    continue;

                auto base = (*iterator).downcast<GPUDecodedFrameData>().configuration();
                configuration_ = Configuration{static_cast<unsigned int>(base.width),
                                               static_cast<unsigned int>(base.height),
                                               0, 0,
                                               base.bitrate, base.framerate,
                                               {static_cast<unsigned int>(0),
                                                static_cast<unsigned int>(0)}};
                return;
            }
            // There should always be at least some data.
            assert(false);
        }

        void updateMaxProcessedFrameNumber(int frameNumber, int parentIndex) {
            if (maxProcessedFrameNumbers_[parentIndex] < frameNumber)
                maxProcessedFrameNumbers_[parentIndex] = frameNumber;
        }

        int minimumProcessedFrameNumber() const {
            return *std::min_element(maxProcessedFrameNumbers_.begin(), maxProcessedFrameNumbers_.end());
        }

        std::vector<GPUFrameReference> pixelsForProcessedRectangles() {
            std::vector<GPUFrameReference> pixelsAsFrames;
            auto findNextProcessedRectangle = [&]() {
                return std::find_if(rectangleToPixels_.begin(), rectangleToPixels_.end(), [&](std::pair<Rectangle, GPUFrameReference> item) {
                    return !rectangleToRemainingCopiesNeeded_[item.first];
                });
            };

            for (auto it = findNextProcessedRectangle(); it != rectangleToPixels_.end(); it = findNextProcessedRectangle()) {
                pixelsAsFrames.push_back(it->second);
                rectangleToRemainingCopiesNeeded_.erase(it->first);
                rectangleToPixels_.erase(it);
            }

            numberOfRectanglesProcessed_ += pixelsAsFrames.size();
            return pixelsAsFrames;
        }

        void copyTilePixelsFromFrameToRectangleFrame(const Rectangle &tileRectangle, GPUFrameReference frame, const Rectangle &objectRectangle, const tiles::TileLayout &tileLayout) {
            // TODO: There are some artifacts from the copying.
            // If we don't have a frame for the rectangle yet, create one.
            GPUFrameReference rectangleFrame = frameForRectangle(objectRectangle, tileLayout);
            --rectangleToRemainingCopiesNeeded_[objectRectangle];

            // Copy the rectangle pixels for the object to the correct location in the rectangle frame.
            // Find the part of the tile that overlaps with the rectangle.
            auto overlappingRectangle = tileRectangle.overlappingRectangle(objectRectangle);
            auto topLeftOffsets = topAndLeftOffsetsOfTileIntoRectangle(tileRectangle, objectRectangle);

            unsigned int codedHeight = frame.is<DecodedFrame>() ? frame.downcast<DecodedFrame>().codedHeight() : 0;

            rectangleFrame.downcast<CudaFrame>().copy(
                    lock(),
                    *frame->cuda(),
                    overlappingRectangle.y - tileRectangle.y,
                    overlappingRectangle.x - tileRectangle.x,
                    topLeftOffsets.first,
                    topLeftOffsets.second,
                    codedHeight);
        }

        std::pair<int, int> topAndLeftOffsetsOfTileIntoRectangle(const Rectangle &tileRectangle, const Rectangle &objectRectangle) {
            auto top = tileRectangle.y <= objectRectangle.y ? 0 : tileRectangle.y - objectRectangle.y;
            auto left = tileRectangle.x <= objectRectangle.x ? 0 : tileRectangle.x - objectRectangle.x;
            return std::make_pair(top, left);
        }

        GPUFrameReference frameForRectangle(const Rectangle &rect, const tiles::TileLayout &tileLayout) {
            if (rectangleToPixels_.count(rect))
                return rectangleToPixels_.at(rect);

            // Create a CudaFrame with the dimensions of the rectangle.
            rectangleToPixels_.emplace(rect, GPUFrameReference::make<CudaFrame>(rect.height, rect.width, NV_ENC_PIC_STRUCT::NV_ENC_PIC_STRUCT_FRAME));
//            auto numberOfTilesForRectangle = tileLayout.tilesForRectangle(rect).size();
            rectangleToRemainingCopiesNeeded_.emplace(rect, tileLayout.tilesForRectangle(rect).size());

            return rectangleToPixels_.at(rect);
        }

        std::vector<int> maxProcessedFrameNumbers_;
        std::unordered_map<Rectangle, GPUFrameReference> rectangleToPixels_;
        std::unordered_map<Rectangle, unsigned int> rectangleToRemainingCopiesNeeded_;
        unsigned int numberOfRectanglesProcessed_;
        bool inNoTilesCase_;
        Configuration configuration_;
        GeometryReference geometry_;
        tiles::TileLayout tileLayoutForNoTilesCase_;

//        int maxProcessedFrameNumber_;
//        int tentativeMaxProcessedFrameNumber_;
    };

    const metadata::MetadataManager &getMetadataManager() {
        if (logical().is<logical::MetadataSubsetLightField>())
            return logical().downcast<logical::MetadataSubsetLightField>().metadataManager();
        else if (logical().is<logical::MetadataSubsetLightFieldWithoutSources>())
            return *logical().downcast<logical::MetadataSubsetLightFieldWithoutSources>().metadataManager();
        else {
            assert(false);
        }
    }

    const metadata::MetadataManager &metadataManager_;
    const tiles::TileLayout tileLayout_;
    std::unordered_map<int, int> parentIndexToTileNumber_;

    std::shared_ptr<tiles::TileLocationProvider> tileLocationProvider_;
};

class CoalesceSingleTile: public PhysicalOperator {
public:
    explicit CoalesceSingleTile(const LightFieldReference &logical,
            const PhysicalOperatorReference &parent,
            const std::vector<int> &expectedFrames,
            const std::vector<int> &allFramesToOutput,
            const catalog::TileEntry &tileEntry,
            unsigned int tileNumber)
        : PhysicalOperator(logical, {parent}, DeviceType::CPU, runtime::make<Runtime>(*this, "CoalesceSingleTile-init")),
        expectedFrames_(expectedFrames),
        allFramesToOutput_(allFramesToOutput),
        tileEntry_(tileEntry),
        tileNumber_(tileNumber)
    { }

    const std::vector<int> &expectedFrames() const { return expectedFrames_; }
    const std::vector<int> &allFramesToOutput() const { return allFramesToOutput_; }
    const catalog::TileEntry &tileEntry() const { return tileEntry_; }
    unsigned int tileNumber() const { return tileNumber_; }

private:
    class Runtime: public runtime::UnaryRuntime<CoalesceSingleTile, CPUEncodedFrameData> {
    public:
        explicit Runtime(CoalesceSingleTile &physical)
            : runtime::UnaryRuntime<CoalesceSingleTile, CPUEncodedFrameData>(physical),
                    expectedFrameIterator_(physical.expectedFrames().begin()),
                    allFramesToOutputIterator_(physical.allFramesToOutput().begin()),
                    blackFramesReader_(physical.tileEntry().pathForBlackTile(physical.tileNumber()), {})
        { }

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos() && !haveMoreGlobalFrames() && blackFramesReader_.isEos())
                return {};

            GLOBAL_TIMER.startSection("CoalesceSingleTile");

            // Read global frames from the black tile until global frames iterator = expected frames iterator.
            auto combinedData = blackFramesData();
            if (combinedData && combinedData->size()) {
                auto returnValue = CPUEncodedFrameData(source().codec(), source().configuration(), source().geometry(), std::move(combinedData));
                GLOBAL_TIMER.endSection("CoalesceSingleTile");
                return returnValue;
            } else {
                if (iterator() != iterator().eos()) {
                    assert(*allFramesToOutputIterator_ == *expectedFrameIterator_);
                    auto data = iterator()++;
                    int firstFrameIndex = 0;
                    assert(data.getFirstFrameIndexIfSet(firstFrameIndex));
                    int numberOfFrames = 0;
                    assert(data.getNumberOfFramesIfSet(numberOfFrames));

                    // Move expected frames iterator forward number of frames amount.
                    if (numberOfFrames) {
                        assert(firstFrameIndex == *expectedFrameIterator_);
                        std::advance(expectedFrameIterator_, numberOfFrames);
                        std::advance(allFramesToOutputIterator_, numberOfFrames);
                    }
                    GLOBAL_TIMER.endSection("CoalesceSingleTile");
                    return data;
                }
            }
            GLOBAL_TIMER.endSection("CoalesceSingleTile");
            return {};
        }

    private:
        bool haveMoreGlobalFrames() const {
            return allFramesToOutputIterator_ != physical().allFramesToOutput().end();
        }

        bool haveMoreExpectedFrames() const {
            return expectedFrameIterator_ != physical().expectedFrames().end();
        }

        const catalog::Source &source() const {
            return physical().tileEntry().sources()[physical().tileNumber()];
        }

        std::unique_ptr<bytestring> blackFramesData() {
            // Create vector of frames to read from a black tile.
            if (!blackFramesReader_.isEos()) {
                return std::move(blackFramesReader_.read()->data());
            } else {
                std::vector<int> blackFramesToRead;
                while (haveMoreGlobalFrames() &&
                       (!haveMoreExpectedFrames() || *allFramesToOutputIterator_ != *expectedFrameIterator_)) {
                    blackFramesToRead.push_back(*allFramesToOutputIterator_++);
                }

                if (!blackFramesToRead.size())
                    return {};

                assert(blackFramesReader_.isEos());
                blackFramesReader_.setNewFrames(blackFramesToRead);
                return std::move(blackFramesReader_.read()->data());
            }
        }

        std::vector<int>::const_iterator expectedFrameIterator_;
        std::vector<int>::const_iterator allFramesToOutputIterator_;
        BlackFrameReader blackFramesReader_;
    };

    std::vector<int> expectedFrames_;
    std::vector<int> allFramesToOutput_;
    const catalog::TileEntry tileEntry_;
    unsigned int tileNumber_;
};

class StitchTiles: public PhysicalOperator {
public:
    explicit StitchTiles(const LightFieldReference &logical,
                         std::vector<PhysicalOperatorReference> &parents,
                         const tiles::TileLayout &tileLayout)
            : PhysicalOperator(logical, parents, DeviceType::CPU, runtime::make<Runtime>(*this, "StitchTiles-init")),
              tileLayout_(tileLayout)
    { }

    const tiles::TileLayout &tileLayout() const { return tileLayout_; }

private:
    class Runtime: public runtime::Runtime<StitchTiles> {
    public:
        explicit Runtime(StitchTiles &physical)
                : runtime::Runtime<StitchTiles>(physical),
                  materializedData_(physical.tileLayout().numberOfTiles()),
                  configuration_(createConfiguration()),
                  geometry_(getGeometry()),
                  parentToTileNumber_(getParentToTileNumber())
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            GLOBAL_TIMER.startSection("StitchTiles");
            if (!all_parent_eos()) {
                // Parent is a CoalesceSingleTile operator, which exposes a tileNumber() function.
                std::vector<bytestring> materializedData(iterators().size());
                for (auto index = 0u; index < iterators().size(); index++) {
                    if (iterators()[index] != iterators()[index].eos()) {
                        bytestring data;
                        while (!data.size()) {
                            auto next = (iterators()[index]++);
                            data = next.downcast<CPUEncodedFrameData>().value();
                        }
                        auto tileIndex = parentToTileNumber_[index];
                        materializedData[tileIndex] = data;
                    }
                }
                auto numberOfRows = physical().tileLayout().numberOfRows();
                auto numberOfColumns = physical().tileLayout().numberOfColumns();
                hevc::StitchContext context({numberOfRows, numberOfColumns},
                                            {configuration_.height / numberOfRows, configuration_.width / numberOfColumns});
                hevc::Stitcher stitcher(context, materializedData);
                auto returnVal = CPUEncodedFrameData(Codec::hevc(), configuration_, geometry_, stitcher.GetStitchedSegments());

                GLOBAL_TIMER.endSection("StitchTiles");
                return returnVal;
            } else {
                GLOBAL_TIMER.endSection("StitchTiles");
                return {};
            }
        }

    private:
        const Configuration createConfiguration() {
            auto configuration = (*iterators().front()).downcast<FrameData>().configuration();
            return Configuration{physical().tileLayout().totalWidth(),
                                 physical().tileLayout().totalHeight(),
                                 configuration.max_width * physical().tileLayout().numberOfColumns(),
                                 configuration.max_height * physical().tileLayout().numberOfRows(),
                                 configuration.bitrate,
                                 configuration.framerate, {}};
        }

        GeometryReference getGeometry() {
            CHECK(!physical().parents().empty());

            return(*iterators().front()).expect_downcast<FrameData>().geometry();
        }

        std::unordered_map<unsigned int, unsigned int> getParentToTileNumber() {
            std::unordered_map<unsigned int, unsigned int> parentToTileNumber;
            for (auto index = 0u; index < physical().parents().size(); ++index) {
                auto tileNumber = physical().parents()[index].downcast<CoalesceSingleTile>().tileNumber();
                parentToTileNumber[index] = tileNumber;
            }
            return parentToTileNumber;
        }

        std::vector<bytestring> materializedData_;
        const Configuration configuration_;
        const GeometryReference geometry_;
        std::unordered_map<unsigned int, unsigned int> parentToTileNumber_;
    };

    tiles::TileLayout tileLayout_;
};

class SaveFramesToFiles: public PhysicalOperator, public GPUOperator {
public:
    explicit SaveFramesToFiles(const LightFieldReference &logical,
            PhysicalOperatorReference &parent)
            : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "SaveFramesToFiles-init")),
            GPUOperator(parent)
    { }

private:
    class Runtime : public runtime::GPUUnaryRuntime<SaveFramesToFiles, GPUDecodedFrameData> {
    public:
        explicit Runtime(SaveFramesToFiles &physical)
            : runtime::GPUUnaryRuntime<SaveFramesToFiles, GPUDecodedFrameData>(physical)
        { }

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos())
                return {};

            auto &decoded = *iterator();
            for (auto &frame : decoded.frames()) {
                createImage(frame->width(), frame->height(), GetLocalImage(*frame->cuda(), frame->width(), frame->height()));
            }
            ++iterator();

            return EmptyData(DeviceType::GPU);
        }

    private:
        std::unique_ptr<bytestring> GetLocalImage(const CudaFrame &source, unsigned int width, unsigned int height) {
                std::unique_ptr<bytestring> data(new bytestring(width * height * 3 / 2, 0));

                CUresult status;
                auto params = CUDA_MEMCPY2D {
                        .srcXInBytes = 0,
                        .srcY = 0,
                        .srcMemoryType = CU_MEMORYTYPE_DEVICE,
                        .srcHost = nullptr,
                        .srcDevice = source.handle(),
                        .srcArray = nullptr,
                        .srcPitch = source.pitch(),

                        .dstXInBytes = 0,
                        .dstY = 0,

                        .dstMemoryType = CU_MEMORYTYPE_HOST,
                        .dstHost = data->data(),
                        .dstDevice = 0,
                        .dstArray = nullptr,
                        .dstPitch = 0,

                        .WidthInBytes = width,
                        .Height = height * 3 / 2
                };

                if((status = cuMemcpy2D(&params)) != CUDA_SUCCESS)
                    throw GpuCudaRuntimeError("Call to cuMemcpy2D failed", status);

                return data;
        }

        void createImage(int width, int height, std::unique_ptr<bytestring> frameData) {
            unsigned int frameSize = width * height;
            unsigned int totalSize = frameSize * 3;

            std::vector<unsigned char> rgb(totalSize);
            std::vector<unsigned char> planes(totalSize);
            std::vector<float> scaled(totalSize);

            auto y_data = reinterpret_cast<const unsigned char*>(frameData->data());
            auto uv_data = y_data + frameSize;
            IppiSize size{static_cast<int>(width), static_cast<int>(height)};
            ippiYCbCr420ToRGB_8u_P2C3R(y_data, width, uv_data, width, rgb.data(),
                                       3 * width, size);//== ippStsNoErr);
            assert(ippiCopy_8u_C3P3R(rgb.data(), 3 * width, std::initializer_list<unsigned char *>(
                    {planes.data(), planes.data() + frameSize, planes.data() + 2 * frameSize}).begin(),
                                     width, size) == ippStsNoErr);
            assert(ippsConvert_8u32f(planes.data(), scaled.data(), totalSize) == ippStsNoErr);
            assert(ippsDivC_32f_I(255.f, scaled.data(), totalSize) == ippStsNoErr);

            auto path = "/home/maureen/frames_to_files/asImage.bin";
            std::ofstream ofs(path, std::ios::out | std::ios::binary);
            ofs.write(reinterpret_cast<char*>(planes.data()), planes.size());
            ofs.close();
        }
    };
};

} // namespace lightdb::physical

#endif //LIGHTDB_TILEOPERATORS_H
