#include "Files.h"
#include "Transaction.h"

namespace lightdb::catalog {
    std::filesystem::path Files::staging_filename(const transactions::Transaction &transaction,
                                                  const catalog::Entry &entry) {
        assert(entry.path().is_absolute());

        auto index = std::count_if(transaction.outputs().begin(), transaction.outputs().end(), [&entry](auto &output) {
            return output.entry().has_value() && output.entry().value().path() == entry.path();
        });

        // ?-i-stream.mp4#tid
        auto filename = std::string("?-") + std::to_string(index) + stream_suffix_ +
                        '#' + std::to_string(transaction.id());
        return entry.path() / filename;
    }

    std::filesystem::path Files::staging_filename(const transactions::Transaction &transaction,
                                                  const std::filesystem::path &path) {
        // foo.mp4#tid
        auto filename = path.filename().string() + '#' + std::to_string(transaction.id());
        return path.parent_path() / filename;
    }

    std::filesystem::path TileFiles::directoryForTilesInFrames(const Entry &entry, unsigned int firstFrame,
                                                           unsigned int lastFrame) {
        // TODO: This won't work if the same frames are stored with different tile layouts. Append UID for now.
        return entry.path() / (std::to_string(firstFrame) + separating_string_ + std::to_string(lastFrame) + separating_string_ + std::to_string(entry.tile_version()));
    }

    std::pair<unsigned int, unsigned int> TileFiles::firstAndLastFramesFromPath(const std::filesystem::path &directoryPath) {
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

    unsigned int TileFiles::tileVersionFromPath(const std::filesystem::path &directoryPath) {
        std::string directoryName = directoryPath.filename();
        auto lastSeparator = directoryName.rfind(separating_string_);
        return std::stoul(directoryName.substr(lastSeparator+1));
    }

} // namespace lightdb::catalog