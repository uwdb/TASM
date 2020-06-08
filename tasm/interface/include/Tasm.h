#ifndef LIGHTDB_TASM_H
#define LIGHTDB_TASM_H

#include <string>

namespace tasm {

class TASM {
public:
    void addMetadata(const std::string &video,
            const std::string &label,
            unsigned int frame,
            unsigned int x1,
            unsigned int y1,
            unsigned int x2,
            unsigned int y2);
};

} // namespace tasm

#endif //LIGHTDB_TASM_H
