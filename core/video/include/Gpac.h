#ifndef LIGHTDB_GPAC_H
#define LIGHTDB_GPAC_H

#include "Transaction.h"
#include <vector>
#include <filesystem>
#include <mutex>

namespace lightdb::video::gpac {
    std::vector<catalog::Source> load_metadata(const std::filesystem::path&, bool strict=true,
                                               const std::optional<Volume>& ={},
                                               const std::optional<GeometryReference>& ={});
    void write_metadata(const std::filesystem::path &metadata_filename,
                        const std::vector<transactions::OutputStream>&);
    bool can_mux(const std::filesystem::path&);
    void mux_media(const std::filesystem::path &source, const std::filesystem::path &destination,
                   const std::optional<Codec>& ={}, bool remove_source=true);

    Configuration load_configuration(const std::filesystem::path &filename);
    void write_tile_configuration(const std::filesystem::path &metadata_filename, const tiles::TileLayout &tileLayouts);
    tiles::TileLayout load_tile_configuration(const std::filesystem::path &metadataFilename);

} // namespace lightdb::video::gpac

#endif //LIGHTDB_GPAC_H
