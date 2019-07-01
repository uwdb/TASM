#ifndef LIGHTDB_BOX_H
#define LIGHTDB_BOX_H

namespace lightdb {
    struct Rectangle {
        unsigned int id;
        unsigned int x, y;
        unsigned int width, height;

        bool containsPoint(const unsigned int &posX, const unsigned int &posY) const {
            return x <= posX
                && y <= posY
                && x + width >= posX
                && y + height >= posY;
        }
    };
} // namespace lightdb

#endif //LIGHTDB_BOX_H
