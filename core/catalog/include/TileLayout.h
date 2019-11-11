#ifndef LIGHTDB_TILELAYOUT_H
#define LIGHTDB_TILELAYOUT_H

#include "Configuration.h"
#include "Rectangle.h"
#include <boost/functional/hash.hpp>
#include <numeric>

namespace lightdb::tiles {

class TileLayout {
public:

    TileLayout(unsigned int numberOfColumns,
            unsigned int numberOfRows,
            const std::vector<unsigned int> &widthsOfColumns,
            const std::vector<unsigned int> &heightsOfRows)
        : numberOfColumns_(numberOfColumns),
        numberOfRows_(numberOfRows),
        widthsOfColumns_(widthsOfColumns),
        heightsOfRows_(heightsOfRows),
        largestWidth_(0),
        largestHeight_(0)
    { }

    TileLayout(const TileLayout &other) = default;

    // TODO: Not sure if this will set the ints correctly.
    TileLayout() = default;

    bool operator==(const TileLayout &other) const {
        return numberOfColumns_ == other.numberOfColumns_
                    && numberOfRows_ == other.numberOfRows_
                    && widthsOfColumns_ == other.widthsOfColumns_
                    && heightsOfRows_ == other.heightsOfRows_;
    }

    bool operator!=(const TileLayout &other) const {
        return numberOfColumns_ != other.numberOfColumns_
                || numberOfRows_ != other.numberOfRows_
                || widthsOfColumns_ != other.widthsOfColumns_
                || heightsOfRows_ != other.heightsOfRows_;
    }

    unsigned int numberOfTiles() const {
        return numberOfColumns_ * numberOfRows_;
    }

    unsigned int numberOfColumns() const {
        return numberOfColumns_;
    }

    unsigned int numberOfRows() const {
        return numberOfRows_;
    }

    const std::vector<unsigned int> &widthsOfColumns() const {
        return widthsOfColumns_;
    }

    const std::vector<unsigned int> &heightsOfRows() const {
        return heightsOfRows_;
    }

    unsigned int totalHeight() const {
        return std::accumulate(heightsOfRows_.begin(), heightsOfRows_.end(), 0);
    }

    unsigned int totalWidth() const {
        return std::accumulate(widthsOfColumns_.begin(), widthsOfColumns_.end(), 0);
    }

    unsigned int largestWidth() const {
        if (largestWidth_)
            return largestWidth_;

        largestWidth_ = *std::max_element(widthsOfColumns_.begin(), widthsOfColumns_.end());
        return largestWidth_;
    }

    unsigned int largestHeight() const {
        if (largestHeight_)
            return largestHeight_;

        largestHeight_ = *std::max_element(heightsOfRows_.begin(), heightsOfRows_.end());
        return largestHeight_;
    }

    Rectangle rectangleForTile(unsigned int tile) const {
        // Figure out what row/column the tile is in.
        unsigned int column = tile % numberOfColumns_;
        unsigned int row = tile / numberOfColumns_;

        // Create bounding rectangle for tile.
        unsigned int leftXForTile = std::accumulate(widthsOfColumns_.begin(), widthsOfColumns_.begin() + column, 0);
        unsigned int topYForTile = std::accumulate(heightsOfRows_.begin(), heightsOfRows_.begin() + row, 0);

        return Rectangle{ 0, leftXForTile, topYForTile, widthsOfColumns_[column], heightsOfRows_[row] };
    }

    std::vector<unsigned int> tilesForRectangle(const Rectangle &rectangle) const;
    std::vector<unsigned int> rectangleIdsThatIntersectTile(const std::vector<Rectangle> &rectangles, unsigned int tile) const {
        Rectangle tileRectangle = rectangleForTile(tile);

        // Create vector of rectangle ids that intersect with the tile's rectangle.
        std::vector<unsigned int> intersectingRectangleIds;
        for (const auto &rectangle : rectangles) {
            if (tileRectangle.intersects(rectangle))
                intersectingRectangleIds.push_back(rectangle.id);
        }

        return intersectingRectangleIds;
    }

private:
    unsigned int tileColumnForX(unsigned int x) const;
    unsigned int tileRowForY(unsigned int y) const;
    unsigned int tileNumberForCoordinate(unsigned int x, unsigned int y) const;

//    std::string catalogEntryName_;
//    std::filesystem::path catalogPath_;
    unsigned int numberOfColumns_;
    unsigned int numberOfRows_;
    std::vector<unsigned int> widthsOfColumns_;
    std::vector<unsigned int> heightsOfRows_;

    mutable unsigned int largestWidth_;
    mutable unsigned int largestHeight_;
//    std::unordered_map<unsigned int, std::filesystem::path> tileToFilePath_;
};

static const TileLayout NoTilesLayout(1, 1, {1}, {1});
static const TileLayout EmptyTileLayout(0, 0, std::vector<unsigned int>(), std::vector<unsigned int>());

static const std::unordered_map<std::string, TileLayout> CatalogEntryToTileLayout {
        { "MVI_63563_tiled", TileLayout(2, 1, {480, 480}, {544}) },
        { "MVI_63563_tiled_custom_gops", TileLayout(2, 1, {480, 480}, {544}) },
        { "MVI_63563_gops_for_tiles", TileLayout(2, 1, {480, 480}, {544}) },
        { "jackson_square_gops_for_tiles", TileLayout(2, 1, {320, 320}, {480}) },
        { "jackson_square_150frame_gops_for_tiles", TileLayout(2, 1, {320, 320}, {480}) },
        { "jackson_square_150frame_680x512_gops_for_tiles", TileLayout(2, 2, {320, 320}, {256, 256}) },
        { "jackson_square_1hr_680x512_gops_for_tiles", TileLayout(2, 2, {320, 320}, {256, 256}) },
        { "MVI_63563_gop30", TileLayout(1, 1, {960}, {544}) },
};

} // namespace lightdb::tiles

namespace std {
template<>
struct hash<lightdb::tiles::TileLayout> {
    size_t operator() (const lightdb::tiles::TileLayout &tileLayout) const {
        size_t seed = 0;
        boost::hash_combine(seed, tileLayout.numberOfColumns());
        boost::hash_combine(seed, tileLayout.numberOfRows());
        boost::hash_combine(seed, tileLayout.widthsOfColumns());
        boost::hash_combine(seed, tileLayout.heightsOfRows());
        return seed;
    }
};
} // namespace std

#endif //LIGHTDB_TILELAYOUT_H
