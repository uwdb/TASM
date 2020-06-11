#ifndef TASM_VIDEO_H
#define TASM_VIDEO_H

#include "Configuration.h"
#include "VideoConfiguration.h"
#include <filesystem>
#include <fstream>

namespace tasm {

static const std::filesystem::path CatalogPath = "resources";

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

class TiledEntry {
public:
    TiledEntry(const std::string &name)
            : name_(name),
            path_(CatalogPath / name_),
            version_(loadVersion())
    {
        if (!std::filesystem::exists(path_))
            std::filesystem::create_directory(path_);
    }

    unsigned int tile_version() const { return version_; }
    const std::filesystem::path &path() const { return path_; }
    const std::string &name() const { return name_; }

    void incrementTileVersion();

private:
    unsigned int loadVersion() {
        auto versionPath = path_ / "tile-version";

        if (std::filesystem::exists(versionPath)) {
            std::ifstream f(versionPath);
            return static_cast<unsigned int>(stoul(std::string(std::istreambuf_iterator<char>(f),
                                                               std::istreambuf_iterator<char>())));
        } else
            return 0u;
    }

    const std::string name_;
    const std::filesystem::path path_;
    unsigned int version_;
};

} // namespace tasm

#endif //TASM_VIDEO_H
