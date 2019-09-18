#include "CrackingOperators.h"

#include "EncodeAPI.h"
#include "Transaction.h"

namespace lightdb::physical {

void CrackVideo::Runtime::reconfigureEncodersForNewLayout(const tiles::TileLayout &newLayout) {
    for (auto tileIndex = 0u; tileIndex < newLayout.numberOfTiles(); ++tileIndex) {
        Rectangle rect = newLayout.rectangleForTile(tileIndex);
        // Don't do this if the configuration is the same as the encoder's current configuration.
        tileEncodersManager_.createEncoderWithConfiguration(tileIndex, rect.width, rect.height);
        tilesCurrentlyBeingEncoded_.push_back(tileIndex);
    }
}

void CrackVideo::Runtime::saveTileGroupsToDisk() {
    // Maybe make new transaction type?
    // TODO: Read should probably be writing to this transaction all along.
    // For now just do it here for ease of debugging.
    transactions::TileCrackingTransaction transaction(physical().catalog(),
                                                    physical().outputEntryName(),
                                                    currentTileLayout_,
                                                    firstFrameInGroup_,
                                                    lastFrameInGroup_);

    // Get the list of tiles that should be involved in the current tile layout.
    // Create an output for each.
    for (auto &tileIndex : tilesCurrentlyBeingEncoded_) {
        auto &output = transaction.write(geometry_,
                                        tileIndex);

        // Write the encoded data for this tile index to the output stream.
        for (auto &data : encodedDataForTiles_[tileIndex])
            output.stream().write(data->data(), data->size());

        encodedDataForTiles_[tileIndex].clear();
    }

    transaction.commit();
}

void CrackVideo::Runtime::readDataFromEncoders(bool shouldFlush) {
    for (auto &i : tilesCurrentlyBeingEncoded_) {
        auto encodedData = shouldFlush ? tileEncodersManager_.flushEncoderForIdentifier(i) : tileEncodersManager_.getEncodedFramesForIdentifier(i);
        if (!encodedData->empty())
            encodedDataForTiles_[i].push_back(std::move(encodedData));
    }
}

void CrackVideo::Runtime::encodeFrameToTiles(GPUFrameReference &frame, int frameNumber) {
    bool shouldBeKeyframe = physical().desiredKeyframes().count(frameNumber);
    tiles::KeyframeConfigurationProvider *keyframeProvider = nullptr;
    if ((keyframeProvider = dynamic_cast<tiles::KeyframeConfigurationProvider *>(tileConfigurationProvider_.get())) != nullptr) {
        shouldBeKeyframe = keyframeProvider->shouldFrameBeKeyframe(frameNumber);
    }
    for (auto &tileIndex : tilesCurrentlyBeingEncoded_) {
        Rectangle rect = currentTileLayout_.rectangleForTile(tileIndex);
        tileEncodersManager_.encodeFrameForIdentifier(tileIndex, *frame, rect.y, rect.x, shouldBeKeyframe);
    }
}

} // namespace lightdb::physical
