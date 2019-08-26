#ifndef LIGHTDB_TILELAYOUT_H
#define LIGHTDB_TILELAYOUT_H

#include "Configuration.h"
#include "Rectangle.h"
#include <numeric>

namespace lightdb::tiles {

class TileLayout {
public:
    TileLayout(const std::string &catalogEntryName,
            unsigned int numberOfColumns,
            unsigned int numberOfRows,
            const Configuration &configuration)
        : numberOfColumns_(numberOfColumns),
        numberOfRows_(numberOfRows),
        widthsOfColumns_(numberOfColumns_, configuration.width / numberOfColumns_),
        heightsOfRows_(numberOfRows_, configuration.height / numberOfRows_)
    { }

    TileLayout(unsigned int numberOfColumns,
            unsigned int numberOfRows,
            const std::vector<unsigned int> &widthsOfColumns,
            const std::vector<unsigned int> &heightsOfRows)
        : numberOfColumns_(numberOfColumns),
        numberOfRows_(numberOfRows),
        widthsOfColumns_(widthsOfColumns),
        heightsOfRows_(heightsOfRows)
    { }

    unsigned int numberOfTiles() const {
        return numberOfColumns_ * numberOfRows_;
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
//    std::unordered_map<unsigned int, std::filesystem::path> tileToFilePath_;
};

static const std::unordered_map<std::string, TileLayout> CatalogEntryToTileLayout {
        { "MVI_63563_tiled", TileLayout(2, 1, {480, 480}, {544}) },
        { "MVI_63563_tiled_custom_gops", TileLayout(2, 1, {480, 480}, {544}) },
};

} // namespace lightdb::tiles

#endif //LIGHTDB_TILELAYOUT_H
