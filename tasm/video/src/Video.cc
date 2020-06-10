#include "Video.h"

#include "Files.h"

namespace tasm {

void CrackedEntry::incrementTileVersion() {
    std::ofstream output(TileFiles::tileVersionFilename(path_));

    ++version_;
    auto newVersionAsString = std::to_string( version_);
    std::copy(newVersionAsString.begin(), newVersionAsString.end(), std::ostreambuf_iterator<char>(output));
}
} // namespace tasm