#ifndef LIGHTDB_TASM_H
#define LIGHTDB_TASM_H

#include "Rectangle.h"
#include "Metadata.h"
#include <list>

namespace lightdb {
class Tasm {
public:
    struct MetadataInfo {
        MetadataInfo(const std::string &video, const std::string &label, int frame, int x1, int y1, int w, int h)
                : video(video), label(label), frame(frame), x1(x1), y1(y1), w(w), h(h)
        {}

        std::string video;
        std::string label;
        int frame;
        int x1;
        int y1;
        int w;
        int h;
    };

    virtual void addMetadata(const std::string &videoId, const std::string &label, int frame, int x, int y, int w, int h) = 0;
    virtual void addMetadata(const std::list<MetadataInfo> &metadataInfo, std::pair<int, int> firstLastFrame) = 0;
    virtual bool detectionHasBeenRunOnFrame(const std::string &videoId, int frame) = 0;
    virtual void markDetectionHasBeenRunOnFrame(const std::string &videoId, int frame) = 0;
    virtual std::shared_ptr<std::vector<Rectangle>> getMetadata(const std::string &videoId, const MetadataSpecification &specification, int frame) = 0;

    virtual ~Tasm() = default;
};
} // namespace lightdb

#endif //LIGHTDB_TASM_H
