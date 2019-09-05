#ifndef LIGHTDB_BOX_H
#define LIGHTDB_BOX_H

#include <boost/functional/hash.hpp>

namespace lightdb {
    struct Rectangle {
        unsigned int id;
        unsigned int x, y;
        unsigned int width, height;

        Rectangle(unsigned int id = 0, unsigned int x = 0, unsigned int y = 0, unsigned int width = 0, unsigned int height = 0)
            : id(id), x(x), y(y), width(width), height(height)
        { }

        bool operator==(const Rectangle &other) const {
            return id == other.id
                    && x == other.x
                    && y == other.y
                    && width == other.width
                    && height == other.height;
        }

        unsigned int area() const {
            return width * height;
        }

        bool containsPoint(const unsigned int &posX, const unsigned int &posY) const {
            return x <= posX
                && y <= posY
                && x + width >= posX
                && y + height >= posY;
        }

        bool intersects(const Rectangle &other) {
            return !(x >= other.x + other.width
                    || other.x >= x + width
                    || y >= other.y + other.height
                    || other.y >= y + height);
        }

        Rectangle overlappingRectangle(const Rectangle &other) const {
            auto top = std::max(y, other.y);
            auto bottom = std::min(y + height, other.y + other.height);
            auto left = std::max(x, other.x);
            auto right = std::min(x + width, other.x + other.width);

            return Rectangle(id, left, top, bottom - top, right - left);
        }
    };
} // namespace lightdb

namespace std {
template<>
struct hash<lightdb::Rectangle> {
    size_t operator()(const lightdb::Rectangle &rect) const {
        size_t seed = 0;
        boost::hash_combine(seed, rect.id);
        boost::hash_combine(seed, rect.x);
        boost::hash_combine(seed, rect.y);
        boost::hash_combine(seed, rect.width);
        boost::hash_combine(seed, rect.height);

        return seed;
    }
};
} // namespace std

#endif //LIGHTDB_BOX_H
