#include "MergeTiles.h"

#include "SemanticDataManager.h"
#include "TileConfigurationProvider.h"

namespace tasm {

static std::pair<int, int> topAndLeftOffsets(const Rectangle &rect, const Rectangle &baseRect) {
    auto top = rect.y <= baseRect.y ? 0 : rect.y - baseRect.y;
    auto left = rect.x <= baseRect.x ? 0 : rect.x - baseRect.x;
    return std::make_pair(top, left);
}

std::optional<GPUPixelDataContainer> MergeTilesOperator::next() {
    auto decodedData = parent_->next();
    if (parent_->isComplete()) {
        assert(!decodedData.has_value());
        isComplete_ = true;
        return std::nullopt;
    }

    assert(decodedData.has_value());

    auto pixelData = std::make_unique<std::vector<GPUPixelDataPtr>>();
    for (auto frame : decodedData->frames()) {
        // Create a pixel object for each bounding box that lies in the decoded tiles.

        int frameNumber;
        assert(frame->getFrameNumber(frameNumber));
        int tileNumber = frame->tileNumber();
        assert(tileNumber != static_cast<int>(-1));

        auto &boundingBoxesForFrame = semanticDataManager_->rectanglesForFrame(frameNumber);
        auto tileRect = tileLayoutProvider_->tileLayoutForFrame(frameNumber).rectangleForTile(tileNumber);

        // TODO: Cache this work. Because it's also done when determining which tiles to decode.
        // See if any of the rectangles intersect this tile.
        for (auto &boundingBox : boundingBoxesForFrame) {
            if (!boundingBox.intersects(tileRect))
                continue;

            auto overlappingRect = tileRect.overlappingRectangle(boundingBox);
            // TODO: Migrate support for objects across tiles.
            assert(overlappingRect == boundingBox);
            auto offsetIntoTile = topAndLeftOffsets(boundingBox, tileRect);

            pixelData->emplace_back(std::make_shared<GPUPixelDataFromDecodedFrame>(
                    frame,
                    boundingBox.width, boundingBox.height,
                    offsetIntoTile.first, offsetIntoTile.second));

        }
    }
    return pixelData;
}

} // namespace tasm
