#ifndef LIGHTDB_TILEOPERATORS_H
#define LIGHTDB_TILEOPERATORS_H

#include "PhysicalOperators.h"
#include "Rectangle.h"

namespace lightdb::physical {

class MergeTilePixels: public PhysicalOperator, public GPUOperator {
public:
    explicit MergeTilePixels(const LightFieldReference &logical,
            std::vector<PhysicalOperatorReference> &parents,
            const tiles::TileLayout &tileLayout,
            std::unordered_map<int, int> parentIndexToTileNumber)
        : PhysicalOperator(logical, parents, DeviceType::GPU, runtime::make<Runtime>(*this, "MergeTilePixels-init")),
        GPUOperator(parents.front()),
        tileLayout_(tileLayout),
        parentIndexToTileNumber_(std::move(parentIndexToTileNumber))
    { }

    const logical::MetadataSubsetLightField &metadataSubsetLightField() const { return logical().downcast<logical::MetadataSubsetLightField>(); }
    const tiles::TileLayout &tileLayout() const { return tileLayout_; }
    int tileNumberForParentIndex(int parentIndex) const { return parentIndexToTileNumber_.at(parentIndex); }

private:
    class Runtime: public runtime::GPURuntime<MergeTilePixels> {
    public:
        explicit Runtime(MergeTilePixels &physical)
            : runtime::GPURuntime<MergeTilePixels>(physical),
                    maxProcessedFrameNumbers_(physical.parents().size(), -1),
                    numberOfRectanglesProcessed_(0),
                    geometry_((*iterators().front()).downcast<GPUDecodedFrameData>().geometry())
        {
            setConfiguration();
//            setGeometry();
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
                        auto tileNumber = physical().tileNumberForParentIndex(index);
                        auto tileRectangle = physical().tileLayout().rectangleForTile(tileNumber);

                        auto decoded = (iterators()[index]++).downcast<GPUDecodedFrameData>();
                        for (auto &frame : decoded.frames()) {
                            // Get rectangles for frame.
                            int frameNumber = -1;
                            assert(frame->getFrameNumber(frameNumber));

                            updateMaxProcessedFrameNumber(frameNumber, index);

                            const auto &rectanglesForFrame = physical().metadataSubsetLightField().rectanglesForFrame(frameNumber);
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

                                copyTilePixelsFromFrameToRectangleFrame(tileRectangle, frame, rectangle);
                            });
                        }

                        if (iterators()[index] == iterators()[index].eos())
                            updateMaxProcessedFrameNumber(INT32_MAX, index);
                    }
                }
            }

            // Return "frames" for rectangles that are on frames that every parent has processed.
            // Find rectangles that have a frame id <= the maximum processed frame, and put those into
            // the "frames" array of a GPUDecodedFramesObject thing.
            auto pixelsAsFrames = pixelsForProcessedRectangles();
            auto returnVal = GPUPixelData(pixelsAsFrames);
//            auto returnVal = GPUDecodedFrameData{configuration_, geometry_, pixelsAsFrames};
            GLOBAL_TIMER.endSection("MergeTilePixels");
            return returnVal;
        }

    private:
        void setConfiguration() {
            auto base = (*iterators().front()).downcast<GPUDecodedFrameData>().configuration();
            configuration_ = Configuration{static_cast<unsigned int>(base.width * 2),
                                           static_cast<unsigned int>(base.height),
                                           0, 0,
                                           base.bitrate, base.framerate,
                                           {static_cast<unsigned int>(0),
                                            static_cast<unsigned int>(0)}};

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
            int maximumFrame = minimumProcessedFrameNumber();
            auto findNextProcessedRectangle = [&]() {
                return std::find_if(rectangleToPixels_.begin(), rectangleToPixels_.end(), [&](std::pair<Rectangle, GPUFrameReference> item) {
                    return static_cast<int>(item.first.id) <= maximumFrame;
                });
            };

            for (auto it = findNextProcessedRectangle(); it != rectangleToPixels_.end(); it = findNextProcessedRectangle()) {
                pixelsAsFrames.push_back(it->second);
                rectangleToPixels_.erase(it);
            }

            numberOfRectanglesProcessed_ += pixelsAsFrames.size();
            return pixelsAsFrames;
        }

        void copyTilePixelsFromFrameToRectangleFrame(const Rectangle &tileRectangle, GPUFrameReference frame, const Rectangle &objectRectangle) {
            // If we don't have a frame for the rectangle yet, create one.
            GPUFrameReference rectangleFrame = frameForRectangle(objectRectangle);

            // Copy the rectangle pixels for the object to the correct location in the rectangle frame.
            // Find the part of the tile that overlaps with the rectangle.
            auto overlappingRectangle = tileRectangle.overlappingRectangle(objectRectangle);
            auto topLeftOffsets = topAndLeftOffsetsOfTileIntoRectangle(tileRectangle, objectRectangle);
            rectangleFrame.downcast<CudaFrame>().copy(
                    lock(),
                    *frame->cuda(),
                    // TODO: Translate overlappingRectangle x & y into non-translated tile coordinates.
                    overlappingRectangle.y - tileRectangle.y,
                    overlappingRectangle.x - tileRectangle.x,
                    topLeftOffsets.first,
                    topLeftOffsets.second);
        }

        std::pair<int, int> topAndLeftOffsetsOfTileIntoRectangle(const Rectangle &tileRectangle, const Rectangle &objectRectangle) {
            auto top = tileRectangle.y <= objectRectangle.y ? 0 : tileRectangle.y - objectRectangle.y;
            auto left = tileRectangle.x <= objectRectangle.x ? 0 : tileRectangle.x - objectRectangle.x;
            return std::make_pair(top, left);
        }

        GPUFrameReference frameForRectangle(const Rectangle &rect) {
            if (rectangleToPixels_.count(rect))
                return rectangleToPixels_.at(rect);

            // Create a CudaFrame with the dimensions of the rectangle.
            rectangleToPixels_.emplace(rect, GPUFrameReference::make<CudaFrame>(rect.height, rect.width, NV_ENC_PIC_STRUCT::NV_ENC_PIC_STRUCT_FRAME));
            return rectangleToPixels_.at(rect);
        }

        std::vector<int> maxProcessedFrameNumbers_;
        std::unordered_map<Rectangle, GPUFrameReference> rectangleToPixels_;
        unsigned int numberOfRectanglesProcessed_;
        Configuration configuration_;
        GeometryReference geometry_;
    };

    const tiles::TileLayout tileLayout_;
    std::unordered_map<int, int> parentIndexToTileNumber_;
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
                    allFramesToOutputIterator_(physical.allFramesToOutput().begin())
        { }

        std::optional<MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos() && !haveMoreGlobalFrames())
                return {};

            GLOBAL_TIMER.startSection("CoalesceSingleTile");

            // Read global frames from the black tile until global frames iterator = expected frames iterator.
            auto combinedData = blackFramesData();

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

                    combinedData.insert(combinedData.end(), data.value().begin(), data.value().end());
                }
            }

            auto returnValue = CPUEncodedFrameData(source().codec(), source().configuration(), source().geometry(), combinedData);

            GLOBAL_TIMER.endSection("CoalesceSingleTile");
            return {returnValue};
        }

    private:
        bool haveMoreGlobalFrames() const {
            return allFramesToOutputIterator_ != physical().allFramesToOutput().end();
        }

        bool haveMoreExpectedFrames() const {
            return expectedFrameIterator_ != physical().expectedFrames().end();
        }

        const catalog::Source source() const {
            return physical().tileEntry().sources()[physical().tileNumber()];
        }

        bytestring blackFramesData() {
            // Create vector of frames to read from a black tile.
            std::vector<int> blackFramesToRead;
            while (haveMoreGlobalFrames() &&
                (!haveMoreExpectedFrames() || *allFramesToOutputIterator_ != *expectedFrameIterator_)) {
                blackFramesToRead.push_back(*allFramesToOutputIterator_++);
            }

            if (!blackFramesToRead.size())
                return {};

            EncodedFrameReader blackFramesReader(
                    physical().tileEntry().pathForBlackTile(physical().tileNumber()),
                    blackFramesToRead);
//            blackFramesReader.setShouldReadFramesExactly(true);

            bytestring blackFramesEncodedData;
            auto numberOfFrames = 0u;

            for (auto frames = blackFramesReader.read(); frames.has_value(); frames = blackFramesReader.read()) {
                blackFramesEncodedData.insert(blackFramesEncodedData.end(), frames->data().begin(), frames->data().end());
                numberOfFrames += frames->numberOfFrames();
            }

            assert(numberOfFrames == blackFramesToRead.size());
            return blackFramesEncodedData;
        }

        std::vector<int>::const_iterator expectedFrameIterator_;
        std::vector<int>::const_iterator allFramesToOutputIterator_;
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
                for (auto index = 0u; index < iterators().size(); index++) {
                    if (iterators()[index] != iterators()[index].eos()) {
                        auto next = (iterators()[index]++);
                        const auto &data = next.downcast<CPUEncodedFrameData>().value();
                        auto tileIndex = parentToTileNumber_[index];
                        materializedData_[tileIndex].insert(materializedData_[tileIndex].end(), data.begin(), data.end());
                    }
                }
                GLOBAL_TIMER.endSection("StitchTiles");
                return CPUEncodedFrameData(Codec::hevc(), configuration_, geometry_, bytestring{});

            } else if (!materializedData_.empty()) {
                auto numberOfRows = physical().tileLayout().numberOfRows();
                auto numberOfColumns = physical().tileLayout().numberOfColumns();
                hevc::StitchContext context({numberOfRows, numberOfColumns},
                                            {configuration_.height / numberOfRows, configuration_.width / numberOfColumns});
                hevc::Stitcher stitcher(context, materializedData_);
                materializedData_.clear(); // for flagging materializedData.empty().

                auto returnValue = CPUEncodedFrameData(Codec::hevc(), configuration_, geometry_, stitcher.GetStitchedSegments());
                GLOBAL_TIMER.endSection("StitchTiles");
                return returnValue;
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

} // namespace lightdb::physical

#endif //LIGHTDB_TILEOPERATORS_H
