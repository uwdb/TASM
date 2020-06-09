#ifndef TASM_VIDEO_H
#define TASM_VIDEO_H

#include "Configuration.h"
#include "VideoConfiguration.h"
#include <filesystem>

namespace tasm {

class Video {
public:
    Video(const std::filesystem::path &path)
        : path_(path),
        configuration_(video::GetConfiguration(path_))
    {}

    const Configuration &configuration() const { return *configuration_; }
    const std::filesystem::path &path() const { return path_; }

private:
    std::filesystem::path path_;
    std::unique_ptr<const Configuration> configuration_;
};

} // namespace tasm

#endif //TASM_VIDEO_H
