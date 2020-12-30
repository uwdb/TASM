#ifndef LIGHTDB_TASM_H
#define LIGHTDB_TASM_H

#include "Rectangle.h"
#include "Metadata.h"

namespace lightdb {
class Tasm {
public:
    virtual void addMetadata(const std::string &videoId, const std::string &label, int frame, int x, int y, int w, int h) = 0;
    virtual bool detectionHasBeenRunOnFrame(const std::string &videoId, int frame) = 0;
    virtual void markDetectionHasBeenRunOnFrame(const std::string &videoId, int frame) = 0;
    virtual std::shared_ptr<std::vector<Rectangle>> getMetadata(const std::string &videoId, const MetadataSpecification &specification, int frame) = 0;

    virtual ~Tasm() = default;
};
} // namespace lightdb

#endif //LIGHTDB_TASM_H
