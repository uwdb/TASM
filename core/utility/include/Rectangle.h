#ifndef LIGHTDB_BOX_H
#define LIGHTDB_BOX_H

namespace lightdb {
    struct Rectangle {
        unsigned int id;
        unsigned int x, y;
        unsigned int width, height;

        Rectangle(unsigned int id = 0, unsigned int x = 0, unsigned int y = 0, unsigned int width = 0, unsigned int height = 0)
            : id(id), x(x), y(y), width(width), height(height)
        { }

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
    };
} // namespace lightdb

#endif //LIGHTDB_BOX_H
