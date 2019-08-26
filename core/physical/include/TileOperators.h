#ifndef LIGHTDB_TILEOPERATORS_H
#define LIGHTDB_TILEOPERATORS_H

#include "PhysicalOperators.h"

namespace lightdb::physical {

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
