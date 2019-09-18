#include "TileLayout.h"

namespace lightdb::tiles {


std::vector<unsigned int> TileLayout::tilesForRectangle(const Rectangle &rectangle) const {
    std::vector<unsigned int> tiles;

    auto leftColumn = tileColumnForX(rectangle.x);
    auto rightColumn = tileColumnForX(rectangle.x + rectangle.width - 1); // The last pixel will be at column width - 1.
    auto topRow = tileRowForY(rectangle.y);
    auto bottomRow = tileRowForY(rectangle.y + rectangle.height - 1); // The last pixel will be at row height - 1.

    // For every row, include all horizontal tiles between left column and right column.
    for (auto column = leftColumn; column <= rightColumn; ++column) {
        for (auto row = topRow; row <= bottomRow; ++row) {
            tiles.emplace_back(column + numberOfColumns_ * row);
        }
    }

    return tiles;
}


unsigned int TileLayout::tileColumnForX(unsigned int x) const {
    unsigned int startingX = 0;
    for (unsigned int i = 0; i < numberOfColumns_; ++i) {
        if (startingX <= x && x < startingX + widthsOfColumns_[i]) {
            return i;
        }
        startingX += widthsOfColumns_[i];
    }

    assert(false);
    return 0;
}

unsigned int TileLayout::tileRowForY(unsigned int y) const {
    unsigned int startingY = 0;
    for (unsigned int i = 0; i < numberOfRows_; ++i) {
        if (startingY <= y && y < startingY + heightsOfRows_[i]) {
            return i;
        }
        startingY += heightsOfRows_[i];
    }

    assert(false);
    return 0;
}


unsigned int TileLayout::tileNumberForCoordinate(unsigned int x, unsigned int y) const {
    // Find horizontal tile that contains x.
    // Ideal would be to have interval tree to go from x -> interval -> tile.
    // For now, O(n) solution of going through vector.
    unsigned int startingX = 0;
    unsigned int horizontalTilePosition = 0;
    for (unsigned int i = 0; i < numberOfColumns_; ++i) {
        if (startingX <= x && x < startingX + widthsOfColumns_[i]) {
            horizontalTilePosition = i;
            break;
        }
        startingX += widthsOfColumns_[i];
    }

    unsigned int startingY = 0;
    unsigned int verticalTilePosition = 0;
    for (unsigned int i = 0; i < numberOfRows_; ++i) {
        if (startingY <= y && y < startingY + heightsOfRows_[i]) {
            verticalTilePosition = i;
            break;
        }
        startingY += heightsOfRows_[i];
    }

    return horizontalTilePosition + verticalTilePosition * numberOfColumns_;
}

} // namespace lightdb::tiles