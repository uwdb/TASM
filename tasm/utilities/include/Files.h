#ifndef TASM_FILES_H
#define TASM_FILES_H

#include "Video.h"

namespace tasm {

class TileFiles {
public:
    static std::experimental::filesystem::path tileVersionFilename(const std::experimental::filesystem::path &path) {
        return path / tile_version_filename_;
    }

    static std::experimental::filesystem::path tileMetadataFilename(const std::experimental::filesystem::path &path) {
        return path / tile_metadata_filename_;
    }

    static std::experimental::filesystem::path directoryForTilesInFrames(const TiledEntry &entry, unsigned int firstFrame,
                                                           unsigned int lastFrame) {
        return entry.path()  / (std::to_string(firstFrame) + separating_string_ + std::to_string(lastFrame) + separating_string_ + std::to_string(entry.tile_version()));
    }

    static std::experimental::filesystem::path temporaryTileFilename(const TiledEntry &entry, unsigned int tileNumber,
                                                       unsigned int firstFrame,
                                                       unsigned int lastFrame) {
        auto tempName = directoryForTilesInFrames(entry, firstFrame, lastFrame) / (baseTileFilename(tileNumber) +
                                                                          temporaryFilenameExtension());
        return tempName;
    }

    static std::experimental::filesystem::path tileFilename(const std::experimental::filesystem::path &directoryPath, unsigned int tileNumber) {
        return directoryPath / (baseTileFilename(tileNumber) + muxedFilenameExtension());
    }

    static std::string muxedFilenameExtension() {
        return ".mp4";
    }

    static std::experimental::filesystem::path tileMetadataFilename(const TiledEntry &entry, unsigned int firstFrame, unsigned lastFrame) {
        return directoryForTilesInFrames(entry, firstFrame, lastFrame) / tile_metadata_filename_;
    }

    static std::pair<unsigned int, unsigned int> firstAndLastFramesFromPath(const std::experimental::filesystem::path &directoryPath) {
        std::string directoryName = directoryPath.filename();

        // First frame = string up to first separating character.
        auto firstSeparator = directoryName.find(separating_string_, 0);

        auto startOfLastFrame = firstSeparator + 1;
        auto secondSeparator = directoryName.find(separating_string_, startOfLastFrame);
        assert(firstSeparator != std::string::npos);
        assert(secondSeparator != std::string::npos);

        unsigned int firstFrame = std::stoul(directoryName.substr(0, firstSeparator));
        unsigned int lastFrame = std::stoul(directoryName.substr(startOfLastFrame, secondSeparator - startOfLastFrame));

        return std::make_pair(firstFrame, lastFrame);
    }

    static unsigned int tileVersionFromPath(const std::experimental::filesystem::path &directoryPath) {
        std::string directoryName = directoryPath.filename();
        auto lastSeparator = directoryName.rfind(separating_string_);
        return std::stoul(directoryName.substr(lastSeparator+1));
    }

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

} // namespace tasm

#endif //TASM_FILES_H
