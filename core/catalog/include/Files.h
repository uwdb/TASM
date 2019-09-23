#ifndef LIGHTDB_FILES_H
#define LIGHTDB_FILES_H

#include "reference.h"
#include <filesystem>

namespace lightdb::transactions {
    class Transaction;
}

namespace lightdb::catalog {

class Entry;

class Files {
public:
    static std::filesystem::path metadata_filename(const std::filesystem::path &path,
                                                   const unsigned int version) {
        return path / (std::to_string(version) + metadata_suffix_); }

    static std::filesystem::path stream_filename(const std::filesystem::path &path, const unsigned int version,
                                                 const unsigned int index) {
        return path / (std::to_string(version) + '-' + std::to_string(index) + stream_suffix_); }

    static std::filesystem::path staging_filename(const transactions::Transaction&, const catalog::Entry&);
    static std::filesystem::path staging_filename(const transactions::Transaction&, const std::filesystem::path&);

    static std::filesystem::path version_filename(const std::filesystem::path &path) {
        return path / version_filename_;
    }

private:
    Files() = default;

    static constexpr auto version_filename_ = "version";
    static constexpr auto metadata_suffix_ = "-metadata.mp4";
    static constexpr auto stream_suffix_ = "-stream.mp4";
};

class TileFiles {
public:
    static std::filesystem::path tileVersionFilename(const std::filesystem::path &path) {
        return path / tile_version_filename_;
    }

    static std::filesystem::path tileMetadataFilename(const std::filesystem::path &path) {
        return path / tile_metadata_filename_;
    }

    static std::filesystem::path directoryForTilesInFrames(const Entry &entry, unsigned int firstFrame,
                                                           unsigned int lastFrame);

    static std::filesystem::path temporaryTileFilename(const Entry &entry, unsigned int tileNumber,
                                                       unsigned int firstFrame,
                                                       unsigned int lastFrame) {
        return directoryForTilesInFrames(entry, firstFrame, lastFrame) / (baseTileFilename(tileNumber) +
                temporaryFilenameExtension());
    }

    static std::filesystem::path tileFilename(const std::filesystem::path &directoryPath, unsigned int tileNumber) {
        return directoryPath / (baseTileFilename(tileNumber) + muxedFilenameExtension());
    }

    static std::string muxedFilenameExtension() {
        return ".mp4";
    }

    static std::filesystem::path tileMetadataFilename(const Entry &entry, unsigned int firstFrame, unsigned lastFrame) {
        return directoryForTilesInFrames(entry, firstFrame, lastFrame) / tile_metadata_filename_;
    }

    static std::pair<unsigned int, unsigned int> firstAndLastFramesFromPath(const std::filesystem::path &directoryPath);

private:
    static std::string temporaryFilenameExtension() {
        return ".hevc";
    }

    static std::string baseTileFilename(unsigned int tileNumber) {
        return "orig-tile-" + std::to_string(tileNumber);
    }

    static constexpr auto tile_version_filename_ = "tile-version";
    static constexpr auto tile_metadata_filename_ = "tile-metadata.bin";
    static constexpr auto separating_string_ = "-";
};

} // namespace lightdb::catalog
#endif //LIGHTDB_FILES_H
