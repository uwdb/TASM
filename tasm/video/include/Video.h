#ifndef TASM_VIDEO_H
#define TASM_VIDEO_H

#include "Configuration.h"
#include "VideoConfiguration.h"
#include <experimental/filesystem>
#include <fstream>
#include <iostream>

namespace tasm {

class CatalogConfiguration {
public:
    static CatalogConfiguration *instance() {
        if (!instance_)
            instance_ = std::unique_ptr<CatalogConfiguration>(new CatalogConfiguration());
        return instance_.get();
    }

    static const std::experimental::filesystem::path &CatalogPath() { return instance()->catalogPath(); }
    static void SetCatalogPath(const std::experimental::filesystem::path &newPath) { instance()->setCatalogPath(newPath); }

    const std::experimental::filesystem::path &catalogPath() const { return catalogPath_; }
    void setCatalogPath(const std::experimental::filesystem::path &newPath) { catalogPath_ = newPath; }

private:
    static std::unique_ptr<CatalogConfiguration> instance_;
    CatalogConfiguration()
        : catalogPath_("resources") {}
    std::experimental::filesystem::path catalogPath_;
};

class Video {
public:
    Video(const std::experimental::filesystem::path &path)
        : path_(path),
        configuration_(video::GetConfiguration(path_))
    {}

    const Configuration &configuration() const { return *configuration_; }
    const std::experimental::filesystem::path &path() const { return path_; }

private:
    std::experimental::filesystem::path path_;
    std::unique_ptr<const Configuration> configuration_;
};

class TiledEntry {
public:
    TiledEntry(const std::string &name, const std::string &metadataIdentifier = "")
            : TiledEntry(name, CatalogConfiguration::CatalogPath() / name, metadataIdentifier)
    {}

    TiledEntry(const std::string &name, const std::experimental::filesystem::path &path, const std::string &metadataIdentifier = "")
        : name_(name),
        metadataIdentifier_(metadataIdentifier.length() ? metadataIdentifier : name),
        path_(path),
        version_(loadVersion())
    {
        if (!std::experimental::filesystem::exists(path_))
            std::experimental::filesystem::create_directory(path_);
    }

    unsigned int tile_version() const { return version_; }
    const std::experimental::filesystem::path &path() const { return path_; }
    const std::string &name() const { return name_; }
    const std::string &metadataIdentifier() const { return metadataIdentifier_; }

    void incrementTileVersion();

private:
    unsigned int loadVersion() {
        auto versionPath = path_ / "tile-version";

        if (std::experimental::filesystem::exists(versionPath)) {
            std::ifstream f(versionPath);
            return static_cast<unsigned int>(stoul(std::string(std::istreambuf_iterator<char>(f),
                                                               std::istreambuf_iterator<char>())));
        } else
            return 0u;
    }

    const std::string name_;
    const std::string metadataIdentifier_;
    const std::experimental::filesystem::path path_;
    unsigned int version_;
};

} // namespace tasm

#endif //TASM_VIDEO_H
