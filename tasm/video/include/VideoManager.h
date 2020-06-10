#ifndef TASM_VIDEOMANAGER_H
#define TASM_VIDEOMANAGER_H

#include <filesystem>

namespace tasm {

class VideoManager {
public:
    VideoManager() {
        createCatalogIfNecessary();
    }

    void store(const std::filesystem::path &path, const std::string &name);


private:
    void createCatalogIfNecessary();
};

} // namespace tasm

#endif //TASM_VIDEOMANAGER_H
