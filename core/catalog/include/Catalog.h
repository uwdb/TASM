#ifndef LIGHTDB_CATALOG_H
#define LIGHTDB_CATALOG_H

#include "Codec.h"
#include "Geometry.h"
#include "Color.h"
#include "Configuration.h"
#include "functional.h"
#include "MP4Reader.h"
#include "TileLayout.h"
#include <filesystem>
#include <utility>
#include <fstream>
#include <mutex>

namespace lightdb {
    class LightField;
    class PhysicalOperator;
    namespace logical { class Algebra; }
    using LightFieldReference = shared_reference<LightField, logical::Algebra>;

    namespace catalog {
        class Entry;
        class TileEntry;
        class MultiTileEntry;

        class Source {
        public:
            Source(const unsigned int index, std::filesystem::path filename,
                   Codec codec, const Configuration &configuration,
                   CompositeVolume volume, const GeometryReference &geometry)
                    : index_(index),
                      filename_(std::move(filename)),
                      codec_(std::move(codec)),
                      configuration_(configuration),
                      volume_(std::move(volume)),
                      geometry_(geometry),
                      mp4Reader_(filename_)
            { }

            unsigned int index() const { return index_; }
            const std::filesystem::path& filename() const { return filename_; }
            const Codec& codec() const { return codec_; }
            const CompositeVolume volume() const { return volume_; }
            const Configuration& configuration() const { return configuration_; }
            const GeometryReference &geometry() const { return geometry_; }
            const MP4Reader &mp4Reader() const { return mp4Reader_; }
            const std::vector<int> &keyframes() const { return mp4Reader_.keyframeNumbers(); }

        private:
            const unsigned int index_;
            const std::filesystem::path filename_;
            const Codec codec_;
            const Configuration configuration_;
            const CompositeVolume volume_;
            const GeometryReference geometry_;

            const MP4Reader mp4Reader_;
        };

        class Catalog {
        public:
            Catalog(const Catalog &catalog) = default;
            explicit Catalog(const std::filesystem::path &path)
                    : Catalog(path, false)
            { }
            explicit Catalog(std::filesystem::path path, const bool create_if_new)
                    : path_(std::move(asserts::CHECK_NONEMPTY(path))) {
                if(create_if_new && !catalog_exists(path))
                    std::filesystem::create_directory(path_);
            }

            static const Catalog &instance()
            {
                if(instance_.has_value())
                    return instance_.value();
                else
                    throw CatalogError("No ambient catalog specified", "instance");
            }
            static const Catalog &instance(Catalog catalog) { return instance_.emplace(catalog); }
            static bool catalog_exists(const std::filesystem::path &path);

            LightFieldReference getTiled(const std::string &name) const;
            LightFieldReference getMultiTiled(const std::string &name) const;
            LightFieldReference getByGOP(const std::string &name) const;
            LightFieldReference get(const std::string &name, bool create=false) const;
            LightFieldReference create(const std::string& name) const;
            bool exists(const std::string &name) const;
            const std::filesystem::path &path() const { return path_; }

        private:
            Catalog() : path_("") { }

            const std::filesystem::path path_;

            inline Entry entry(const std::string &name) const;
            TileEntry tileEntry(const std::string &name) const;
            MultiTileEntry multiTileEntry(const std::string &name) const;

            static std::optional<Catalog> instance_;
        };

        class TileEntry {
            friend class Catalog;

        public:
            const std::string &name() const { return name_; }
            const Volume &volume() const { return volume_; }
            const ColorSpace &colorSpace() const { return colorSpace_; }
            const tiles::TileLayout &tileLayout() const { return tileLayout_; }
            const std::vector<Source> &sources() const { return sources_; }
            std::filesystem::path pathForBlackTile(unsigned int tile) const;

        private:
            TileEntry(const Catalog &catalog, std::string name, std::filesystem::path path)
                : catalog_(catalog),
                name_(std::move(name)),
                path_(std::move(path)),
                colorSpace_(YUVColorSpace::instance()),
                volume_(Volume::limits()),
                tileLayout_(tiles::CatalogEntryToTileLayout.at(name_)),
                sources_(loadSources())
            {
                CHECK(std::filesystem::exists(path_));
            }

            std::vector<Source> loadSources();
            std::filesystem::path pathForOriginalTile(unsigned int tile) const;

            const Catalog &catalog_;
            const std::string name_;
            const std::filesystem::path path_;
            const ColorSpace colorSpace_;
            const Volume volume_;
            const tiles::TileLayout tileLayout_;
            const std::vector<Source> sources_;
        };

        class MultiTileEntry {
            friend class Catalog;

        public:
            const std::string &name() const { return name_; }
            const catalog::Catalog &catalog() const { return catalog_; }
            const std::filesystem::path &path() const { return path_; }

        private:
            MultiTileEntry(const Catalog &catalog, std::string name, std::filesystem::path path)
                : catalog_(catalog),
                name_(std::move(name)),
                path_(std::move(path))
            { }

            const Catalog &catalog_;
            const std::string name_;
            const std::filesystem::path path_;

            // TODO: Add something for frame range -> tile layout.
        };

        class Entry {
            friend class Catalog;

            Entry(const Catalog &catalog, std::string name, std::filesystem::path path)
                    : catalog_(catalog),
                      name_(std::move(name)),
                      path_(std::move(path)),
                      colorSpace_(YUVColorSpace::instance()),
                      version_(load_version(path_)),
                      sources_(load_sources()),
                      volume_(CompositeVolume(functional::transform<CompositeVolume>(sources_.begin(), sources_.end(),
                              [](const auto &source) { return source.volume(); }), Volume::zero()).bounding()),
                      tileVersion_(load_tile_version(path_))
            { CHECK(std::filesystem::exists(path_)); }

        public:
            inline const std::string& name() const noexcept { return name_; }
            inline const Catalog& catalog() const noexcept { return catalog_; }
            inline const Volume& volume() const noexcept { return volume_; }
            inline const ColorSpace& colorSpace() const noexcept { return colorSpace_; }
            inline const std::filesystem::path path() const noexcept { return path_; }
            inline const std::vector<Source>& sources() const noexcept { return sources_; }
            inline unsigned int version() const { return version_; }
            inline unsigned int version(const unsigned int version) { return version_ = version; }

            static unsigned int load_version(const std::filesystem::path &path);
            static unsigned int write_version(const std::filesystem::path &path, unsigned int version);
            static unsigned int increment_version(const std::filesystem::path &path);

            static unsigned int load_tile_version(const std::filesystem::path &path);
            static void increment_tile_version(const std::filesystem::path &path);
            unsigned int tile_version() const { return tileVersion_; }

        private:
            std::vector<Source> load_sources();

            const Catalog &catalog_;
            const std::string name_;
            const std::filesystem::path path_;
            const ColorSpace colorSpace_;
            unsigned int version_;
            std::vector<Source> sources_;
            const Volume volume_;

            unsigned int tileVersion_;
        };

        class ExternalEntry {
        public:
            explicit ExternalEntry(const std::filesystem::path &filename)
                    : ExternalEntry(filename, {}, {})
            { }

            explicit ExternalEntry(std::filesystem::path filename, const std::optional<Volume> &volume,
                                                                   const std::optional<GeometryReference> &geometry)
                    : filename_(std::move(filename)),
                      sources_(load_sources(volume, geometry))
            { CHECK(std::filesystem::exists(filename_)); }

            inline const std::filesystem::path &filename() const noexcept { return filename_; }
            inline const std::vector<Source>& sources() const noexcept { return sources_; }

        private:
            std::vector<Source> load_sources(const std::optional<Volume>&, const std::optional<GeometryReference>&);

            const std::filesystem::path filename_;
            const std::vector<Source> sources_;
        };
    } //namespace catalog
} //namespace lightdb

#endif //LIGHTDB_CATALOG_H
