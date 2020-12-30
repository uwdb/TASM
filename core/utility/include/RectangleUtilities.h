#ifndef LIGHTDB_RECTANGLEUTILITIES_H
#define LIGHTDB_RECTANGLEUTILITIES_H

#include "Rectangle.h"

#include <boost/functional/hash.hpp>

namespace lightdb {
    class RectangleMerger {
    public:
        RectangleMerger(std::unique_ptr<std::list<Rectangle>> rectangles)
            : rectangles_(std::move(rectangles))
        {
            merge();
        }

        void addRectangle(const Rectangle &other) {
            auto merged = false;
            for (auto &rectangle : *rectangles_) {
                if (rectangle.intersects(other)) {
                    rectangle.expand(other);
                    merged = true;
                }
            }
            if (!merged)
                rectangles_->push_back(other);

            merge();
        }

        const std::list<Rectangle> &rectangles() const { return *rectangles_; }

    private:
        void merge() {
            bool changed = true;
            while (changed) {
                changed = false;
                for (auto it = rectangles_->begin(); it != rectangles_->end(); ++it) {
                    for (auto it2 = std::next(it, 1); it2 != rectangles_->end(); ++it2) {
                        if (it->intersects(*it2)) {
                            it->expand(*it2);
                            rectangles_->erase(it2);
                            changed = true;
                            break;
                        }
                    }
                    if (changed)
                        break;
                }
            }
        }

        std::unique_ptr<std::list<Rectangle>> rectangles_;
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

#endif //LIGHTDB_RECTANGLEUTILITIES_H
