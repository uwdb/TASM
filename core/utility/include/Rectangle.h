#ifndef LIGHTDB_BOX_H
#define LIGHTDB_BOX_H

namespace lightdb {

template<class T>
const T& max(const T&a, const T& b) {
    return (a < b) ? b : a;
}

template<class T>
const T& min(const T&a, const T& b) {
    return (a < b) ? a : b;
}

struct Rectangle {
    unsigned int id;
    unsigned int x, y;
    unsigned int width, height;

    Rectangle(unsigned int id = 0, unsigned int x = 0, unsigned int y = 0, unsigned int width = 0,
              unsigned int height = 0)
            : id(id), x(x % 2 ? x - 1 : x), y(y % 2 ? y - 1 : y), width(width % 2 ? width + 1 : width),
              height(height % 2 ? height + 1 : height) {}

    bool operator==(const Rectangle &other) const {
        return id == other.id
               && x == other.x
               && y == other.y
               && width == other.width
               && height == other.height;
    }

    bool hasEqualDimensions(const Rectangle &other) const {
        return width == other.width
               && height == other.height;
    }

    unsigned int area() const {
        return width * height;
    }

    bool containsPoint(const unsigned int &posX, const unsigned int &posY) const {
        return x <= posX
               && y <= posY
               && x + width > posX
               && y + height > posY;
    }

    bool intersects(const Rectangle &other) const {
        bool val = !(x >= other.x + other.width
                     || other.x >= x + width
                     || y >= other.y + other.height
                     || other.y >= y + height);

        return val;
    }

    Rectangle overlappingRectangle(const Rectangle &other) const {
        int top = max(y, other.y);
        int bottom = min(y + height, other.y + other.height);
        int left = max(x, other.x);
        int right = min(x + width, other.x + other.width);

        return Rectangle(id, left, top, right - left, bottom - top);
    }

    Rectangle expand(const Rectangle &other) {
        int left = min(x, other.x);
        int right = max(x + width, other.x + other.width);
        x = left;
        width = right - left;

        int top = min(y, other.y);
        int bottom = max(y + height, other.y + other.height);
        y = top;
        height = bottom - top;

        return *this;
    }
};
} // namespace lightdb
#endif //LIGHTDB_BOX_H
