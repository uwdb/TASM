//
// Created by Maureen Daum on 2019-06-21.
//

#include "MetadataLightField.h"

namespace lightdb::physical {

    std::vector<Rectangle> MetadataLightField::rectanglesForLabel(const std::string &label) {
        // FIXME: Should only make this once.
        std::vector<Rectangle> rectangles;

        std::multimap<std::string, Rectangle> allLabels = metadata_.at("labels");
        std::pair<std::multimap<std::string, Rectangle>::iterator, std::multimap<std::string, Rectangle>::iterator> rectanglesForLabel
            = allLabels.equal_range(label);

        for (std::multimap<std::string, Rectangle>::iterator it = rectanglesForLabel.first; it != rectanglesForLabel.second; ++it) {
            rectangles.push_back(it->second);
        }

        return rectangles;
    }

} // namespace lightdb::physical
