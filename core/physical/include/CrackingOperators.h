#ifndef LIGHTDB_CRACKINGOPERATORS_H
#define LIGHTDB_CRACKINGOPERATORS_H

#include "EncodeWriter.h"
#include "PhysicalOperators.h"
#include "TileConfigurationProvider.h"
#include "VideoEncoder.h"
#include "VideoEncoderSession.h"

namespace lightdb::physical {

class TileEncoder {
public:
    TileEncoder(EncodeConfiguration encodeConfiguration, GPUContext &context, VideoLock &lock)
            : encodeConfiguration_(std::move(encodeConfiguration)),
              encoder_{context, encodeConfiguration_, lock},
              writer_{encoder_.api()},
              encodeSession_{encoder_, writer_}
    { }

    void updateConfiguration(unsigned int newWidth, unsigned int newHeight);
    bytestring getEncodedFrames();
    void encodeFrame(Frame &frame, unsigned int top, unsigned int left, bool isKeyframe);
    void flush();

private:
    EncodeConfiguration encodeConfiguration_;
    VideoEncoder encoder_;
    MemoryEncodeWriter writer_;
    VideoEncoderSession encodeSession_;
};

class CrackVideo : public PhysicalOperator, public GPUOperator {
public:
    // TODO: Cracking operator should know about catalog entry in order to create necessary directory/names.
    explicit CrackVideo(const LightFieldReference &logical,
            PhysicalOperatorReference &parent,
            std::unordered_set<int> desiredKeyframes)
        : PhysicalOperator(logical, {parent}, DeviceType::GPU, runtime::make<Runtime>(*this, "CrackVideo-init")),
        GPUOperator(parent),
        outputEntryName_(logical.downcast<logical::CrackedLightField>().name()),
        desiredKeyframes_(std::move(desiredKeyframes))
    { }

    const std::string &outputEntryName() const { return outputEntryName_; }
    const catalog::Catalog &catalog() const { return logical().downcast<logical::CrackedLightField>().catalog(); }
    const std::unordered_set<int> &desiredKeyframes() const { return desiredKeyframes_; }

private:
    class Runtime : public runtime::GPUUnaryRuntime<CrackVideo, GPUDecodedFrameData> {
    public:
        explicit Runtime(CrackVideo &physical)
            : runtime::GPUUnaryRuntime<CrackVideo, GPUDecodedFrameData>(physical),
                    geometry_(getGeometry()),
                    tileConfigurationProvider_(std::make_unique<tiles::GOP30TileConfigurationProvider>()),
                    firstFrameInGroup_(-1),
                    lastFrameInGroup_(-1),
                    frameNumber_(0),
                    encodedDataForTiles_(tileConfigurationProvider_->maximumNumberOfTiles())
        {
            // Create a tile encoder for each of the maximum number of tiles.
            auto numberOfTiles = tileConfigurationProvider_->maximumNumberOfTiles();
            tileEncoders_.reserve(numberOfTiles);
            for (auto i = 0u; i < numberOfTiles; ++i) {
                // Configuration should have width of full video, which is the maximum possible width of any tile.
                tileEncoders_.emplace_back(EncodeConfiguration(configuration(), Codec::hevc().nvidiaId().value(), 1000),
                        context(),
                        lock());
            }
        }

        std::optional<physical::MaterializedLightFieldReference> read() override {
            if (iterator() == iterator().eos()) {
                // TODO: Make sure that all encoders are done, and everything is saved to disk.
                // Flush encoders.
                flushAllEncoders();

                // Read encoded data.
                readEncodedData();

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
                    // This results in each encoder flushing its data.
                    reconfigureEncodersForNewLayout(tileLayout);

                    // Read the data that was flushed from the encoders because it has the rest of the frames that were
                    // encoded with the last configuration.
                    readEncodedData();

                    // TODO: Save frames from last GOP.
                    // Get rest of encoded frames from each encoder & save.
                    // Save with first and last frames, tile number, tile layout.
                    // Save after
                    if (firstFrameInGroup_ != -1)
                        saveTileGroupsToDisk();

                    currentTileLayout_ = tileLayout;

                    firstFrameInGroup_ = frameNumber;
                }

                // Encode each tile for this frame.
                // Set the width/height/top/left parts of the configuration to control what will get encoded.
                lastFrameInGroup_ = frameNumber;
                encodeFrameToTiles(frame, frameNumber);
            }

            // Move information from encode buffers so they don't fill up.
            readEncodedData();

            ++iterator();

            GLOBAL_TIMER.endSection("CrackVideo");
            return EmptyData{physical().device()};
        }

    private:
        void reconfigureEncodersForNewLayout(const tiles::TileLayout &newLayout);
        void saveTileGroupsToDisk();
        void encodeFrameToTiles(GPUFrameReference &frame, int frameNumber);
        void readEncodedData();
        void flushAllEncoders() {
            std::for_each(tileEncoders_.begin(), tileEncoders_.end(), std::mem_fun_ref(&TileEncoder::flush));
        }

        GeometryReference getGeometry() {
            return geometry();
        }

        const GeometryReference geometry_;
        std::unique_ptr<tiles::TileConfigurationProvider> tileConfigurationProvider_;
        std::vector<TileEncoder> tileEncoders_;
        tiles::TileLayout currentTileLayout_;
        int firstFrameInGroup_;
        int lastFrameInGroup_;
        unsigned int frameNumber_;

        // This should actually probably be a list of output streams related to the current tile group's transaction.
        std::vector<std::list<bytestring>> encodedDataForTiles_;
    };
    const std::string outputEntryName_;
    std::unordered_set<int> desiredKeyframes_;
};

} // namespace lightdb::physical

#endif //LIGHTDB_CRACKINGOPERATORS_H
