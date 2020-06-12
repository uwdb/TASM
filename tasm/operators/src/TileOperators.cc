#include "TileOperators.h"

#include "EncodeAPI.h"
#include "Transaction.h"

namespace tasm {

std::optional<GPUDecodedFrameData> TileOperator::next() {
    auto decodedData = parent_->next();
    if (parent_->isComplete()) {
        readDataFromEncoders(true);
        saveTileGroupsToDisk();
        isComplete_ = true;
        return {};
    }

    for (auto frame : decodedData->frames()) {
        int frameNumber = -1;
        frameNumber = frame->getFrameNumber(frameNumber) ? frameNumber : frameNumber_++;
        auto tileLayout = tileConfigurationProvider_->tileLayoutForFrame(frameNumber);
        assert(tileLayout != EmptyTileLayout);

        // Reconfigure the encoders if the layout changed.
        if (!currentTileLayout_ || tileLayout != *currentTileLayout_ || frameNumber != lastFrameInGroup_ + 1) {
            // Read the data that was flushed from the encoders because it has the rest of the frames
            // that were encoded with the last configuration.
            if (currentTileLayout_) {
                readDataFromEncoders(true);
                if (firstFrameInGroup_ != -1)
                    saveTileGroupsToDisk();
            }
            tilesCurrentlyBeingEncoded_.clear();

            // Reconfigure the encoders.
            reconfigureEncodersForNewLayout(tileLayout);

            currentTileLayout_.reset(new TileLayout(tileLayout));
            firstFrameInGroup_ = frameNumber;
        }

        // Encode each tile.
        lastFrameInGroup_ = frameNumber;
        encodeFrameToTiles(frame, frameNumber);
    }

    return decodedData;
}

void TileOperator::reconfigureEncodersForNewLayout(const tasm::TileLayout &newLayout) {
    for (auto tileIndex = 0u; tileIndex < newLayout.numberOfTiles(); ++tileIndex) {
        Rectangle rect = newLayout.rectangleForTile(tileIndex);
        tileEncodersManager_.createEncoderWithConfiguration(tileIndex, rect.width, rect.height);
        tilesCurrentlyBeingEncoded_.push_back(tileIndex);
    }
}

void TileOperator::saveTileGroupsToDisk() {
    if (!currentTileLayout_ || *currentTileLayout_ == EmptyTileLayout) {
        return;
    }

    assert(tilesCurrentlyBeingEncoded_.size());
    TileCrackingTransaction transaction(outputEntry_,
                                      *currentTileLayout_,
                                      firstFrameInGroup_,
                                      lastFrameInGroup_);

    // Get the list of tiles that should be involved in the current tile layout.
    // Create an output for each.
    for (auto &tileIndex : tilesCurrentlyBeingEncoded_) {
        auto &output = transaction.write(tileIndex);

        // Write the encoded data for this tile index to the output stream.
        for (auto &data : encodedDataForTiles_[tileIndex])
            output.stream().write(data->data(), data->size());

        encodedDataForTiles_[tileIndex].clear();
    }

    transaction.commit();
}

void TileOperator::readDataFromEncoders(bool shouldFlush) {
    for (auto &i : tilesCurrentlyBeingEncoded_) {
        auto encodedData = shouldFlush ? tileEncodersManager_.flushEncoderForIdentifier(i) : tileEncodersManager_.getEncodedFramesForIdentifier(i);
        if (!encodedData->empty())
            encodedDataForTiles_[i].push_back(std::move(encodedData));
    }
}

void TileOperator::encodeFrameToTiles(GPUFramePtr frame, int frameNumber) {
    for (auto &tileIndex : tilesCurrentlyBeingEncoded_) {
        Rectangle rect = currentTileLayout_->rectangleForTile(tileIndex);
        tileEncodersManager_.encodeFrameForIdentifier(tileIndex, *frame, rect.y, rect.x, false);
    }
}


} // namespace tasm
