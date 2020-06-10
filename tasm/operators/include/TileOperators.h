#ifndef TASM_TILEOPERATORS_H
#define TASM_TILEOPERATORS_H

#include "EncodedData.h"
#include "Files.h"
#include "MultipleEncoderManager.h"
#include "Operator.h"
#include "TileConfigurationProvider.h"
#include "Video.h"

namespace tasm {

class TileOperator : public Operator<GPUDecodedFrameData> {
public:
    TileOperator(std::shared_ptr<Video> video,
            std::shared_ptr<Operator<GPUDecodedFrameData>> parent,
            std::shared_ptr<TileLayoutProvider> tileConfigurationProvider,
            std::string outputEntryName,
            unsigned int layoutDuration,
            std::shared_ptr<GPUContext> context,
            std::shared_ptr<VideoLock> lock)
            : isComplete_(false),
            video_(video),
            parent_(parent),
            tileConfigurationProvider_(tileConfigurationProvider),
          outputEntry_(new CrackedEntry(CatalogPath / outputEntryName)),
          layoutDuration_(layoutDuration),
          tileEncodersManager_(EncodeConfiguration(parent->configuration(), NV_ENC_HEVC, layoutDuration), *context, *lock),
          firstFrameInGroup_(-1),
          lastFrameInGroup_(-1),
          frameNumber_(0)
    {}

    bool isComplete() override { return isComplete_; }
    const Configuration &configuration() override { return parent_->configuration(); }
    std::optional<GPUDecodedFrameData> next() override;

private:
    void reconfigureEncodersForNewLayout(const TileLayout &newLayout);
    void saveTileGroupsToDisk();
    void encodeFrameToTiles(GPUFramePtr frame, int frameNumber);
    void readDataFromEncoders(bool shouldFlush);

    bool isComplete_;
    std::shared_ptr<Video> video_;
    std::shared_ptr<Operator<GPUDecodedFrameData>> parent_;
    std::shared_ptr<TileLayoutProvider> tileConfigurationProvider_;
    std::shared_ptr<CrackedEntry> outputEntry_;
    const unsigned int layoutDuration_;
    MultipleEncoderManager tileEncodersManager_;
    std::unique_ptr<TileLayout> currentTileLayout_;
    int firstFrameInGroup_;
    int lastFrameInGroup_;
    unsigned int frameNumber_;
    std::vector<unsigned int> tilesCurrentlyBeingEncoded_;

    std::unordered_map<unsigned int, std::list<std::unique_ptr<std::vector<char>>>> encodedDataForTiles_;
};

} // namespace tasm

#endif //TASM_TILEOPERATORS_H
