#ifndef TASM_ENVIRONMENTCONFIGURATION_H
#define TASM_ENVIRONMENTCONFIGURATION_H

#include <experimental/filesystem>
#include <optional>
#include <unordered_map>

namespace tasm {

class EnvironmentConfiguration {
public:
    static constexpr auto DefaultLabelsDB = "default_db_path";
    static constexpr auto CatalogPath = "catalog_path";
    EnvironmentConfiguration(const std::unordered_map<std::string, std::string> &configOptions = {})
        : labelsDatabasePath_(configOptions.count(DefaultLabelsDB) ? configOptions.at(DefaultLabelsDB) : defaultDBPath),
        catalogPath_(configOptions.count(CatalogPath) ? configOptions.at(CatalogPath) : defaultCatalogPath)
    { }

    const std::experimental::filesystem::path &defaultLabelsDatabasePath() const { return labelsDatabasePath_; };
    const std::experimental::filesystem::path &catalogPath() const { return catalogPath_; }

    static const EnvironmentConfiguration & instance() {
        if (instance_.has_value())
            return *instance_;

        instance_ = std::make_optional<EnvironmentConfiguration>();
        return *instance_;
    }

    static const EnvironmentConfiguration &instance(EnvironmentConfiguration config) { return instance_.emplace(config); }

private:
    std::experimental::filesystem::path labelsDatabasePath_;
    std::experimental::filesystem::path catalogPath_;
    static constexpr auto defaultDBPath = "labels.db";
    static constexpr auto defaultCatalogPath = "resources";

    static std::optional<EnvironmentConfiguration> instance_;
};

} // namespace tasm

#endif //TASM_ENVIRONMENTCONFIGURATION_H
