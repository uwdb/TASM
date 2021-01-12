#ifndef LIGHTDB_CRACKINGOPERATORS_H
#define LIGHTDB_CRACKINGOPERATORS_H

#include "MultipleEncoderManager.h"
#include "PhysicalOperators.h"
#include "TileConfigurationProvider.h"
#include "ThreadPool.h"
#include "Transaction.h"
#include <iostream>

namespace lightdb::physical {

class CrackVideo : public PhysicalOperator, public GPUOperator {
public:
    // TODO: Cracking operator should know about catalog entry in order to create necessary directory/names.
    explicit CrackVideo(const LightFieldReference &logical,
            PhysicalOperatorReference &parent,
            std::unordered_set<int> desiredKeyframes,
            std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider,
            std::string outputEntryName = "",
            unsigned int gopLength = 0,
            bool splitByGOP = false)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "CrackVideo-init", tileConfigurationProvider, splitByGOP)),
        GPUOperator(parent),
        outputEntryName_(outputEntryName.length() ? outputEntryName : logical.downcast<logical::CrackedLightField>().name()),
        desiredKeyframes_(std::move(desiredKeyframes)),
        tileConfigurationProvider_(tileConfigurationProvider),
        gopLength_(gopLength)
    { }

    const std::string &outputEntryName() const { return outputEntryName_; }
    const catalog::Catalog &catalog() const {
        return logical().is<logical::CrackedLightField>()
                    ? logical().downcast<logical::CrackedLightField>().catalog()
                    : catalog::Catalog::instance();
    }
    const std::unordered_set<int> &desiredKeyframes() const { return desiredKeyframes_; }
    unsigned int gopLength() const { return gopLength_; }

private:
    class Runtime : public runtime::GPURuntime<CrackVideo> {
    public:
        explicit Runtime(CrackVideo &physical, std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider, bool splitByGOP)
            : runtime::GPURuntime<CrackVideo>(physical),
                    geometry_(getGeometry()),
                    tileConfigurationProvider_(tileConfigurationProvider),
                    tileEncodersManager_(EncodeConfiguration((*iterators().front()).downcast<GPUDecodedFrameData>().configuration(), Codec::hevc().nvidiaId().value(), physical.gopLength() ?: 1000), context(), lock()),
                    firstFrameInGroup_(-1),
                    lastFrameInGroup_(-1),
                    frameNumber_(0),
                    currentGOP_(-1),
                    splitByGOP_(splitByGOP)
//                    threadPool_(context(), 1)
        { }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            assert(iterators().size() == 1);

            if (iterators().front() == iterators().front().eos()) {
                // Read encoded data.
                GLOBAL_TIMER.startSection("CrackVideo");
//                threadPool_.push([this]() {
                    readDataFromEncoders(true);

                    // Save data to disk.
                    saveTileGroupsToDisk();
//                });

//                threadPool_.waitAll(); // This blocks the last iteration of merge.

                GLOBAL_TIMER.endSection("CrackVideo");
                return {};
            }

            auto decodedReference = iterators().front()++;

            GLOBAL_TIMER.startSection("CrackVideo");

//            threadPool_.push([this, decodedReference]() {
                auto &decoded = decodedReference.downcast<GPUDecodedFrameData>();
                for (auto &frame: decoded.frames()) {
                    // Get the tile configuration for this frame.
                    // If it is different than the previous tile configuration, reconfigure the necessary encoders.
                    long frameNumber = -1;
                    // If we are scanning the entire file, there won't be frame numbers attached to the decoded frames.
                    frameNumber = frame->getFrameNumber(frameNumber) ? frameNumber : frameNumber_++;
                    auto &tileLayout = tileConfigurationProvider_->tileLayoutForFrame(frameNumber);
                    auto gop = frameNumber / physical().gopLength();

                    // See if the current frame is already the correct layout.
                    // If so, and if there are frames that are currently being encoded, save them to disk.
                    // Don't reconfigure the encoder.
                    // Set current tile layout to empty tile layout. That way when the next frame comes in that actually does
                    // need cracking, it won't re-save frames to disk, and even if the tile layout is the same as this one,
                    // it will start a new GOP and directory because some frames will be skipped.
                    if (frame.is<DecodedFrame>()) {
                        auto &decodedFrame = frame.downcast<DecodedFrame>();
                        auto tileNumber = decodedFrame.tileNumber();
                        // If the tile number has been set, it means this frame has been cracked before.
                        // See if its dimensions equal those that are specified by the tile layout.
                        if (tileNumber >= 0) {
                            auto desiredTileRect = tileLayout.rectangleForTile(tileNumber);
                            if ((desiredTileRect.width == decodedFrame.width() &&
                                desiredTileRect.height == decodedFrame.height())
                                    || (decodedFrame.width() < tileLayout.totalWidth()
                                            || decodedFrame.height() < tileLayout.totalHeight())) {
                                // We don't have any more cracking to do.
                                // Flush the encoders because this represents a gap in the cracked frames.
                                if (currentTileLayout_ != tiles::EmptyTileLayout) {
                                    readDataFromEncoders(true);
                                    if (firstFrameInGroup_ != -1)
                                        saveTileGroupsToDisk();

                                    tilesCurrentlyBeingEncoded_.clear();
                                }

                                currentTileLayout_ = tiles::EmptyTileLayout;
                                continue; // Move on to the next decoded frame.
                            }
                        }
                    }

                    if (tileLayout != currentTileLayout_ || frameNumber != lastFrameInGroup_ + 1 || (splitByGOP_ && gop != currentGOP_)) {
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
                        currentGOP_ = gop;
                    }

                    // Encode each tile for this frame.
                    lastFrameInGroup_ = frameNumber;
                    if (currentTileLayout_ != tiles::EmptyTileLayout)
                        encodeFrameToTiles(frame, frameNumber);
                }

                // Move information from encode buffers so they don't fill up.
                readDataFromEncoders(false);
//            });

//            ++iterator();

            GLOBAL_TIMER.endSection("CrackVideo");
            return decodedReference;
//            return EmptyData{physical().device()};
        }

    private:
        void reconfigureEncodersForNewLayout(const tiles::TileLayout &newLayout);
        void saveTileGroupsToDisk();
        void encodeFrameToTiles(const GPUFrameReference &frame, int frameNumber);
        void readDataFromEncoders(bool shouldFlush);

        GeometryReference getGeometry() {
            return (*iterators().front()).downcast<GPUDecodedFrameData>().geometry();
        }

        const GeometryReference geometry_;
        std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider_;
        MultipleEncoderManager tileEncodersManager_;
        tiles::TileLayout currentTileLayout_;
        int firstFrameInGroup_;
        int lastFrameInGroup_;
        unsigned int frameNumber_;
        int currentGOP_;
        bool splitByGOP_;
        std::vector<unsigned int> tilesCurrentlyBeingEncoded_;

        // This should actually probably be a list of output streams related to the current tile group's transaction.
        std::unordered_map<unsigned int, std::list<std::unique_ptr<bytestring>>> encodedDataForTiles_;

//        GPUThreadPool threadPool_;
    };
    const std::string outputEntryName_;
    std::unordered_set<int> desiredKeyframes_;
    std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider_;
    unsigned int gopLength_;
};

class CrackVideoWithoutEncoding {
public:
    explicit CrackVideoWithoutEncoding(
            const LightFieldReference &scanned,
            const std::string &metadataIdentifier,
            const MetadataSpecification &metadataSpecification,
            unsigned int layoutDuration,
            CrackingStrategy strategy,
            std::string outputEntryName = "")
        : tileConfigurationProvider_(createProvider(scanned, strategy, std::make_shared<metadata::MetadataManager>(metadataIdentifier, metadataSpecification), layoutDuration)),
        nb_frames_(scanned.downcast<logical::ScannedLightField>().sources()[0].nb_frames()),
        outputEntryName_(outputEntryName)
    {}

    void saveTileLayouts() {
        auto firstFrameInGroup = 0;
        auto lastFrameInGroup = 0;
        tiles::TileLayout currentTileLayout;
        for (auto i = 0u; i < nb_frames_; ++i) {
            auto &tileLayout = tileConfigurationProvider_->tileLayoutForFrame(i);
            if (!i)
                currentTileLayout = tileLayout;
            else if (tileLayout != currentTileLayout) {
                saveTileLayoutToDisk(currentTileLayout, firstFrameInGroup, lastFrameInGroup);
                firstFrameInGroup = i;
                currentTileLayout = tileLayout;
            }
            lastFrameInGroup = i;
        }
        saveTileLayoutToDisk(currentTileLayout, firstFrameInGroup, lastFrameInGroup);
    }

    const std::string &outputEntryName() const { return outputEntryName_; }
    const catalog::Catalog &catalog() const {
        return catalog::Catalog::instance();
    }

private:
    std::shared_ptr<tiles::TileConfigurationProvider> createProvider(const LightFieldReference &scan, CrackingStrategy strategy, std::shared_ptr<metadata::MetadataManager> metadataManager, unsigned int layoutDuration) {
        auto &config = scan.downcast<logical::ScannedLightField>().sources()[0].configuration();
        if (strategy == CrackingStrategy::SmallTiles) {
            return std::make_shared<tiles::GroupingTileConfigurationProvider>(
                    layoutDuration,
                    metadataManager,
                    config.width,
                    config.height);
        } else if (strategy == CrackingStrategy::GroupingExtent) {
            return std::make_shared<tiles::GroupingExtentsTileConfigurationProvider>(
                    layoutDuration,
                    metadataManager,
                    config.width,
                    config.height);
        } else {
            assert(false);
        }
    }

    void saveTileLayoutToDisk(const tiles::TileLayout &layout, int firstFrame, int lastFrame) {
        transactions::TileCrackingTransaction transaction(catalog(),
                                                          outputEntryName(),
                                                          layout,
                                                          firstFrame,
                                                          lastFrame);
        transaction.commit();
    }

    std::shared_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider_;
    unsigned int nb_frames_;
    const std::string outputEntryName_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_CRACKINGOPERATORS_H
