#ifndef LIGHTDB_CRACKINGOPERATORS_H
#define LIGHTDB_CRACKINGOPERATORS_H

#include "MultipleEncoderManager.h"
#include "PhysicalOperators.h"
#include "TileConfigurationProvider.h"

namespace lightdb::physical {

class CrackVideo : public PhysicalOperator, public GPUOperator {
public:
    // TODO: Cracking operator should know about catalog entry in order to create necessary directory/names.
    explicit CrackVideo(const LightFieldReference &logical,
            PhysicalOperatorReference &parent,
            std::unordered_set<int> desiredKeyframes,
            std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "CrackVideo-init", tileConfigurationProvider)),
        GPUOperator(parent),
        outputEntryName_(logical.downcast<logical::CrackedLightField>().name()),
        desiredKeyframes_(std::move(desiredKeyframes)),
        tileConfigurationProvider_(tileConfigurationProvider)
    { }

    const std::string &outputEntryName() const { return outputEntryName_; }
    const catalog::Catalog &catalog() const { return logical().downcast<logical::CrackedLightField>().catalog(); }
    const std::unordered_set<int> &desiredKeyframes() const { return desiredKeyframes_; }

private:
    class Runtime : public runtime::GPUUnaryRuntime<CrackVideo, GPUDecodedFrameData> {
    public:
        explicit Runtime(CrackVideo &physical, std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider)
            : runtime::GPUUnaryRuntime<CrackVideo, GPUDecodedFrameData>(physical),
                    geometry_(getGeometry()),
                    tileConfigurationProvider_(tileConfigurationProvider),
                    tileEncodersManager_(EncodeConfiguration(configuration(), Codec::hevc().nvidiaId().value(), 1000), context(), lock()),
                    firstFrameInGroup_(-1),
                    lastFrameInGroup_(-1),
                    frameNumber_(0)
        {
            // TODO: Keep track of which tiles are in play at any given time.
            // Communicate with tileEncodersManager_ for those tiles.

            // Create a tile encoder for each of the maximum number of tiles.
//            auto numberOfTiles = tileConfigurationProvider_->maximumNumberOfTiles();
//            tileEncoders_.reserve(numberOfTiles);
//            for (auto i = 0u; i < numberOfTiles; ++i) {
//                // Configuration should have width of full video, which is the maximum possible width of any tile.
//                tileEncoders_.emplace_back(EncodeConfiguration(configuration(), Codec::hevc().nvidiaId().value(), 1000),
//                        context(),
//                        lock());
//            }
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos()) {
                // Read encoded data.
                readDataFromEncoders(true);

                // Save data to disk.
                saveTileGroupsToDisk();

                return {};
            }

            GLOBAL_TIMER.startSection("CrackVideo");
            auto &decoded = *iterator();

            for (auto &frame: decoded.frames()) {
                // Get the tile configuration for this frame.
                // If it is different than the previous tile configuration, reconfigure the necessary encoders.
                int frameNumber = -1;
                // If we are scanning the entire file, there won't be frame numbers attached to the decoded frames.
                frameNumber = frame->getFrameNumber(frameNumber) ? frameNumber : frameNumber_++;
                auto &tileLayout = tileConfigurationProvider_->tileLayoutForFrame(frameNumber);
                if (tileLayout != currentTileLayout_) {
                    // Read the data that was flushed from the encoders because it has the rest of the frames that were
                    // encoded with the last configuration.
                    // Only save data if we were actually encoding it.
                    if (currentTileLayout_ != tiles::EmptyTileLayout) {
                        readDataFromEncoders(true);
                        if (firstFrameInGroup_ != -1)
                            saveTileGroupsToDisk();
                    }
                    tilesCurrentlyBeingEncoded_.clear();

                    // Only reconfigure the encoders if we will actually be encoding frames.
                    if (tileLayout != tiles::EmptyTileLayout)
                        reconfigureEncodersForNewLayout(tileLayout);

                    currentTileLayout_ = tileLayout;
                    firstFrameInGroup_ = frameNumber;
                }

                // Encode each tile for this frame.
                lastFrameInGroup_ = frameNumber;
                if (currentTileLayout_ != tiles::EmptyTileLayout)
                    encodeFrameToTiles(frame, frameNumber);
            }

            // Move information from encode buffers so they don't fill up.
            readDataFromEncoders(false);

            ++iterator();

            GLOBAL_TIMER.endSection("CrackVideo");
            return EmptyData{physical().device()};
        }

    private:
        void reconfigureEncodersForNewLayout(const tiles::TileLayout &newLayout);
        void saveTileGroupsToDisk();
        void encodeFrameToTiles(GPUFrameReference &frame, int frameNumber);
        void readDataFromEncoders(bool shouldFlush);

        GeometryReference getGeometry() {
            return geometry();
        }

        const GeometryReference geometry_;
        std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider_;
        MultipleEncoderManager tileEncodersManager_;
        tiles::TileLayout currentTileLayout_;
        int firstFrameInGroup_;
        int lastFrameInGroup_;
        unsigned int frameNumber_;
        std::vector<unsigned int> tilesCurrentlyBeingEncoded_;

        // This should actually probably be a list of output streams related to the current tile group's transaction.
        std::unordered_map<unsigned int, std::list<std::unique_ptr<bytestring>>> encodedDataForTiles_;
    };
    const std::string outputEntryName_;
    std::unordered_set<int> desiredKeyframes_;
    std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_CRACKINGOPERATORS_H
