#ifndef TASM_VIDEOCONFIGURATION_H
#define TASM_VIDEOCONFIGURATION_H

#include "Configuration.h"

#include <filesystem>

namespace tasm::video {

std::unique_ptr<Configuration> GetConfiguration(const std::filesystem::path &path);

} // namespace tasm::video

#endif //TASM_VIDEOCONFIGURATION_H
