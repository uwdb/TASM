#ifndef TASM_GPAC_H
#define TASM_GPAC_H

#include "TileLayout.h"
#include <experimental/filesystem>

namespace tasm::gpac {
void mux_media(const std::experimental::filesystem::path &source, const std::experimental::filesystem::path &destination);
void write_tile_configuration(const std::experimental::filesystem::path &metadata_filename, const TileLayout &tileLayouts);
TileLayout load_tile_configuration(const std::experimental::filesystem::path &metadataFilename);
} // namespace tasm::gpac

#endif //TASM_GPAC_H
