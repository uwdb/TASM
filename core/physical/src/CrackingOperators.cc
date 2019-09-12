#include "CrackingOperators.h"

#include "EncodeAPI.h"
#include "Transaction.h"

namespace lightdb::physical {

void TileEncoder::updateConfiguration(unsigned int newWidth, unsigned int newHeight) {
    NvEncPictureCommand picCommand;
    picCommand.bResolutionChangePending = true;
    picCommand.newWidth = newWidth;
    picCommand.newHeight = newHeight;

    auto result = encoder_.api().NvEncReconfigureEncoder(&picCommand);
    assert(result == NV_ENC_SUCCESS);
}

bytestring TileEncoder::getEncodedFrames() {
    unsigned int numberOfFrames;
    return writer_.dequeue(numberOfFrames);
}

void TileEncoder::encodeFrame(Frame &frame, unsigned int top, unsigned int left, bool isKeyframe) {
    encodeSession_.Encode(frame, top, left, isKeyframe);
}

void TileEncoder::flush() {
    encodeSession_.Flush();
}

void CrackVideo::Runtime::reconfigureEncodersForNewLayout(const tiles::TileLayout &newLayout) {
    for (auto tileIndex = 0u; tileIndex < newLayout.numberOfTiles(); ++tileIndex) {
        Rectangle rect = newLayout.rectangleForTile(tileIndex);
        // Don't do this if the configuration is the same as the encoder's current configuration.
        tileEncoders_[tileIndex].updateConfiguration(rect.width, rect.height);
    }
    flushAllEncoders();
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
    for (auto tileIndex = 0u; tileIndex < currentTileLayout_.numberOfTiles(); ++tileIndex) {
        auto &output = transaction.write(geometry_,
                                        tileIndex);

        // Write the encoded data for this tile index to the output stream.
        for (auto &data : encodedDataForTiles_[tileIndex])
            output.stream().write(data.data(), data.size());

        encodedDataForTiles_[tileIndex].clear();
    }

    for (auto tileIndex = currentTileLayout_.numberOfTiles(); tileIndex < encodedDataForTiles_.size(); ++tileIndex)
        assert(encodedDataForTiles_[tileIndex].empty());

    transaction.commit();
}

void CrackVideo::Runtime::readEncodedData() {
    for (auto i = 0u; i < encodedDataForTiles_.size(); ++i) {
        // I don't think it's an issue if the data is empty?
        // But might be better not to push it so we can assert that tiles that shouldn't be in group of tiles are
        // actually empty.
        auto encodedData = tileEncoders_[i].getEncodedFrames();
        if (!encodedData.empty())
            encodedDataForTiles_[i].push_back(std::move(encodedData));
    }

}

void CrackVideo::Runtime::encodeFrameToTiles(GPUFrameReference &frame, int frameNumber) {
    for (auto tileIndex = 0u; tileIndex < currentTileLayout_.numberOfTiles(); ++tileIndex) {
        Rectangle rect = currentTileLayout_.rectangleForTile(tileIndex);
        tileEncoders_[tileIndex].encodeFrame(*frame, rect.y, rect.x, physical().desiredKeyframes().count(frameNumber));
    }
}

} // namespace lightdb::physical
