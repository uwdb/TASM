#ifndef TASM_VIDEOMANAGER_H
#define TASM_VIDEOMANAGER_H

#include <filesystem>

namespace tasm {

class VideoManager {
public:
    void store(const std::filesystem::path &path, const std::string &name);
};

} // namespace tasm

#endif //TASM_VIDEOMANAGER_H
