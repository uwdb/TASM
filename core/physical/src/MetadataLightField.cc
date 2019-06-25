//
// Created by Maureen Daum on 2019-06-21.
//

#include "MetadataLightField.h"

namespace lightdb::physical {

    std::vector<Rectangle> MetadataLightField::allRectangles() {
        return metadata_.at("labels");
    }

} // namespace lightdb::physical
