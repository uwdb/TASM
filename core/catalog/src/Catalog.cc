#include "Catalog.h"
#include "LightField.h"
#include "PhysicalOperators.h"
#include "Model.h"
#include "Transaction.h"
#include "Gpac.h"

namespace lightdb::catalog {
    std::optional<Catalog> Catalog::instance_;

    bool Catalog::catalog_exists(const std::filesystem::path &path) {
        return std::filesystem::exists(path) &&
               std::filesystem::is_directory(path);
    }

    bool Catalog::exists(const std::string &name) const {
        auto path = std::filesystem::absolute(path_ / name);
        return std::filesystem::exists(path) &&
               std::filesystem::is_directory(path);
    }

    Entry Catalog::entry(const std::string &name) const {
        auto path = std::filesystem::absolute(path_ / name);
        return Entry{*this, name, path};
    }

    TileEntry Catalog::tileEntry(const std::string &name) const {
        auto path = std::filesystem::absolute(path_ / name);
        return TileEntry(*this, name, path);
    }

    unsigned int Entry::load_version(const std::filesystem::path &path) {
        auto filename = Files::version_filename(path);

        if(std::filesystem::exists(filename)) {
            std::ifstream f(filename);
            return static_cast<unsigned int>(stoul(std::string(std::istreambuf_iterator<char>(f),
                                                   std::istreambuf_iterator<char>())));
        } else
            return 0u;
    }

    unsigned int Entry::write_version(const std::filesystem::path &path, const unsigned int version) {
        auto string = std::to_string(version);

        std::ofstream output(catalog::Files::version_filename(path));
        std::copy(string.begin(), string.end(), std::ostreambuf_iterator<char>(output));

        return version;
    }

    unsigned int Entry::increment_version(const std::filesystem::path &path) {
        return write_version(path, load_version(path) + 1);
    }

    unsigned int Entry::load_tile_version(const std::filesystem::path &path) {
        auto filename = TileFiles::tileVersionFilename(path);

        if (std::filesystem::exists(filename)) {
            std::ifstream f(filename);
            return static_cast<unsigned int>(stoul(std::string(std::istreambuf_iterator<char>(f),
                                                                      std::istreambuf_iterator<char>())));
        } else
            return 0u;
    }

    void Entry::increment_tile_version(const std::filesystem::path &path) {
        auto tileVersion = load_tile_version(path);

        // Write incremented version.
        std::ofstream output(TileFiles::tileVersionFilename(path));
        auto newVersionAsString = std::to_string(tileVersion + 1);
        std::copy(newVersionAsString.begin(), newVersionAsString.end(), std::ostreambuf_iterator<char>(output));
    }


    std::vector<Source> Entry::load_sources() {
        auto filename = Files::metadata_filename(path(), version());

        return std::filesystem::exists(filename)
            ? video::gpac::load_metadata(filename)
            : std::vector<Source>{};
    }

    LightFieldReference Catalog::getTiled(const std::string &name) const {
        assert(exists(name));
        return LightFieldReference::make<logical::ScannedTiledLightField>(std::move(tileEntry(name)));
    }

    LightFieldReference Catalog::getByGOP(const std::string &name) const {
        assert(exists(name));
        return LightFieldReference::make<logical::ScannedByGOPLightField>(entry(name));
    }

    LightFieldReference Catalog::get(const std::string &name, const bool create) const {
        if(exists(name))
            return LightFieldReference::make<logical::ScannedLightField>(entry(name));
        else if(create)
            return this->create(name);
        else
            throw CatalogError(std::string("Light field '") + name + "' does not exist in catalog " + path_.string(), name);
    }

    LightFieldReference Catalog::create(const std::string& name) const {
        auto path = std::filesystem::absolute(path_ / name);

        if(!std::filesystem::exists(path))
            std::filesystem::create_directory(path);
        else
            throw CatalogError("Entry already exists", name);

        return LightFieldReference::make<logical::ScannedLightField>(Entry{*this, name, path});
    }

    std::vector<Source> ExternalEntry::load_sources(const std::optional<Volume>& volume,
                                                    const std::optional<GeometryReference>& geometry) {
        if(video::gpac::can_mux(filename()))
            return video::gpac::load_metadata(filename(), false, volume, geometry);
        else if(!volume.has_value())
            throw CatalogError("Volume must be supplied for external files with no metadata", filename());
        else if(!geometry.has_value())
            throw CatalogError("Geometry must be supplied for external files with no metadata", filename());
        else if (filename().extension() == ".boxes") {
            return { Source{0, filename(), Codec::boxes(), Configuration(), volume.value(), geometry.value()} };
        }
        else
            return functional::transform<Source>(video::ffmpeg::GetStreamConfigurations(filename(), true),
                    [this, &volume, &geometry](const auto &configuration) {
                        return Source{
                            0, filename(), configuration.decode.codec, configuration.decode,
                            volume.value() | TemporalRange{volume->t().start(),
                                                           volume->t().start() + configuration.duration},
                            geometry.value()}; });
    }

    std::vector<Source> TileEntry::loadSources() {
        GeometryReference geometry = GeometryReference::make<EquirectangularGeometry>(EquirectangularGeometry::Samples());

        auto numberOfTiles = tileLayout_.numberOfTiles();

        // For now assume that all tiles are identical. We can't get configurations for later tiles
        // because their streams are invalid.
        auto firstTileStreamConfigurations = video::ffmpeg::GetStreamConfigurations(pathForOriginalTile(0), true);
        assert(firstTileStreamConfigurations.size() == 1);
        auto configuration = firstTileStreamConfigurations[0];

        std::vector<Source> sources;
        sources.reserve(numberOfTiles);
        for (auto i = 0u; i < numberOfTiles; ++i) {
            auto pathForTile = pathForOriginalTile(i);
            sources.emplace_back(i,
                    pathForTile,
                    configuration.decode.codec,
                    configuration.decode,
                    volume_ | TemporalRange{volume_.t().start(), volume_.t().start() + configuration.duration},
                    geometry);
        }

        return sources;
    }

    std::filesystem::path TileEntry::pathForOriginalTile(unsigned int tile) const {
        assert(tile < tileLayout().numberOfTiles());

        return std::filesystem::absolute(path_ / ("orig-tile-" + std::to_string(tile) + ".mp4"));
    }

    std::filesystem::path TileEntry::pathForBlackTile(unsigned int tile) const {
        assert(tile < tileLayout().numberOfTiles());

        return std::filesystem::absolute(path_ / ("black-tile-" + std::to_string(tile) + ".mp4"));
    }
} // namespace lightdb::catalog