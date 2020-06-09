#ifndef LIGHTDB_VIDEOCONFIGURATION_H
#define LIGHTDB_VIDEOCONFIGURATION_H

#include "Configuration.h"

#include <filesystem>

namespace tasm::video {

std::unique_ptr<const Configuration> GetConfiguration(const std::filesystem::path &path);

} // namespace tasm::video

#endif //LIGHTDB_VIDEOCONFIGURATION_H
