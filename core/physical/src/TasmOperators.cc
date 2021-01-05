#include "TasmOperators.h"

#include "stitcher/TileStitcher.h"

namespace lightdb::physical {

static const unsigned int MAX_PPS_ID = 64;
static const unsigned int ALIGNMENT = 32;

static std::unique_ptr<bytestring> ReadFile(const std::filesystem::path &path) {
    std::ifstream istrm(path);
    return std::make_unique<bytestring>(std::istreambuf_iterator<char>(istrm), std::istreambuf_iterator<char>());
}

static std::vector<unsigned int> ToCtbs(const std::vector<unsigned int> &pixelVals) {
    std::vector<unsigned int> ctbs(pixelVals.size() - 1);
    std::transform(pixelVals.begin(), std::prev(pixelVals.end(), 1), ctbs.begin(), [](auto &v) { return std::ceil(v / ALIGNMENT); });
    return ctbs;
}

std::shared_ptr<bytestring> StitchOperator::Runtime::getSEI() {
    if (sei_)
        return sei_;

    sei_ = std::shared_ptr(ReadFile(seiPath_));
    return sei_;
}

std::shared_ptr<bytestring> StitchOperator::Runtime::stitchGOP(int gop) {
    // First, get tile layout for the current GOP.
    auto firstFrameOfGOP = gop * gopLength_;
    auto tileLayout = tileLocationProvider_->tileLayoutForFrame(firstFrameOfGOP);

    // Special case when there is one tile:
    // Send special SEI followed by the single tile.
    if (tileLayout.numberOfTiles() == 1) {
        if (!sentSEIForOneTile_) {
            sentSEIForOneTile_ = true;
            return getSEI();
        } else {
            sentSEIForOneTile_ = false;
            return ReadFile(tileLocationProvider_->locationOfTileForFrame(0, firstFrameOfGOP));
        }
    }

    // First: create context.
    int tileDimensions[2] = {static_cast<int>(tileLayout.numberOfRows()), static_cast<int>(tileLayout.numberOfColumns())};
    int videoCodedDimensions[2] = {static_cast<int>(tileLayout.codedHeight()), static_cast<int>(tileLayout.codedWidth())};
    int videoDisplayDimensions[2] = {static_cast<int>(tileLayout.totalHeight()), static_cast<int>(tileLayout.totalWidth())};
    bool shouldUseUniformTiles = false;
    tiles::Context context(tileDimensions,
            videoCodedDimensions,
            videoDisplayDimensions,
            shouldUseUniformTiles,
            ToCtbs(tileLayout.heightsOfRows()),
            ToCtbs(tileLayout.widthsOfColumns()),
            ppsId_++);

    // Respect limits on PPS_ID in HEVC specification.
    if (ppsId_ >= MAX_PPS_ID)
        ppsId_ = 1;

    // Second: read tiles into memory
    std::list<std::unique_ptr<bytestring>> tiles;
    for (auto i = 0u; i < tileLayout.numberOfTiles(); ++i)
        tiles.push_back(ReadFile(tileLocationProvider_->locationOfTileForFrame(i, firstFrameOfGOP)));

    tiles::Stitcher<std::list> stitcher(context, tiles);
    return stitcher.GetStitchedSegments();
}

} // namespace lightdb::physical
