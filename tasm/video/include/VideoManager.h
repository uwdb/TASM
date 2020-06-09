#ifndef LIGHTDB_VIDEOMANAGER_H
#define LIGHTDB_VIDEOMANAGER_H

#include <filesystem>

namespace tasm {

class VideoManager {
public:
    void store(const std::filesystem::path &path, const std::string &name);
};

} // namespace tasm

#endif //LIGHTDB_VIDEOMANAGER_H
