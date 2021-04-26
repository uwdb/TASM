#include "Gpac.h"

#include "TileConfiguration.pb.h"
#include "gpac/isomedia.h"
#include "gpac/media_tools.h"
#include <fstream>

namespace tasm::gpac {

static auto constexpr TILE_CONFIGURATION_VERSION = 1u;

void mux_media(const std::experimental::filesystem::path &source, const std::experimental::filesystem::path &destination) {
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
    else if(!std::experimental::filesystem::remove(source))
        throw std::runtime_error("Error deleting source file");
}

} // namespace tasm::gpac