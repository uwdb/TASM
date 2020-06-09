#include "Tasm.h"

namespace tasm {

void TASM::addMetadata(const std::string &video,
                       const std::string &label,
                       unsigned int frame,
                       unsigned int x1,
                       unsigned int y1,
                       unsigned int x2,
                       unsigned int y2) {
    semanticIndex_->addMetadata(video, label, frame, x1, y1, x2, y2);
}

} // namespace tasm