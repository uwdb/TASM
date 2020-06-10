#include "VideoManager.h"

#include "ScanOperators.h"
#include "DecodeOperators.h"
#include "TileOperators.h"
#include "Video.h"

namespace tasm {

void VideoManager::store(const std::filesystem::path &path, const std::string &name) {
    std::shared_ptr<Video> video(new Video(path));

    auto gpuContext = std::make_shared<GPUContext>(0);
    auto lock = std::make_shared<VideoLock>(gpuContext);

    std::shared_ptr<ScanFileDecodeReader> scan(new ScanFileDecodeReader(video));
    std::shared_ptr<GPUDecodeFromCPU> decode(new GPUDecodeFromCPU(scan, video->configuration(), gpuContext, lock));

    auto tileConfigurationProvider = std::make_shared<SingleTileConfigurationProvider>(
            video->configuration().displayWidth,
            video->configuration().displayHeight);
    TileOperator tile(video, decode, tileConfigurationProvider, name, video->configuration().frameRate, gpuContext, lock);
    while (!tile.isComplete()) {
        tile.next();
    }
}

} // namespace tasm