//
// Created by Maureen Daum on 2019-06-21.
//

#include "MetadataLightField.h"

namespace lightdb::physical {

    std::vector<Rectangle> MetadataLightField::allRectangles() {
        std::vector<Rectangle> allRectangles;
        std::for_each(metadata_.begin(), metadata_.end(), [&](std::pair<std::string, std::vector<Rectangle>> labelAndRectangles) {
           allRectangles.insert(allRectangles.end(), labelAndRectangles.second.begin(), labelAndRectangles.second.end());
        });

        return allRectangles;
    }

} // namespace lightdb::physical
