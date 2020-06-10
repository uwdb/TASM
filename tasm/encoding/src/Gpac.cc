#include "Gpac.h"

#include "TileConfiguration.pb.h"
#include "gpac/isomedia.h"
#include "gpac/media_tools.h"
#include <fstream>

namespace tasm::gpac {

static auto constexpr TILE_CONFIGURATION_VERSION = 1u;

void mux_media(const std::filesystem::path &source, const std::filesystem::path &destination) {
    GF_MediaImporter import{};
    GF_Err result;
    auto input = source.string();
    auto extension = source.extension().string();

    import.in_name = input.data();
    import.force_ext = extension.data();

    if((import.dest = gf_isom_open(destination.c_str(), GF_ISOM_OPEN_WRITE, nullptr)) == nullptr)
        throw std::runtime_error("Error opening destination file");
    else if((result = gf_media_import(&import)) != GF_OK)
        throw std::runtime_error("Error importing track: " + std::to_string(result));
    else if((result = gf_isom_close(import.dest)) != GF_OK)
        throw std::runtime_error("Error closing file: " + std::to_string(result));
    else if(!std::filesystem::remove(source))
        throw std::runtime_error("Error deleting source file");
}

static void write_tile_configuration(const std::filesystem::path &metadata_filename,
                                     const lightdb::serialization::TileConfiguration &tileConfiguration) {
    std::fstream output(metadata_filename, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!tileConfiguration.SerializeToOstream(&output))
        throw std::runtime_error("Failed to write tile configuration to file");
}

void write_tile_configuration(const std::filesystem::path &metadata_filename,
                              const TileLayout &tileLayout) {
    lightdb::serialization::TileConfiguration tileConfiguration;
    tileConfiguration.set_version(TILE_CONFIGURATION_VERSION);

    auto numberOfColumns = tileLayout.numberOfColumns();
    auto numberOfRows = tileLayout.numberOfRows();

    tileConfiguration.set_numberofcolumns(numberOfColumns);
    tileConfiguration.set_numberofrows(numberOfRows);

    for (auto &width : tileLayout.widthsOfColumns())
        tileConfiguration.add_widthsofcolumns(width);

    for (auto &height : tileLayout.heightsOfRows())
        tileConfiguration.add_heightsofrows(height);

    write_tile_configuration(metadata_filename, tileConfiguration);
}

} // namespace tasm::gpac