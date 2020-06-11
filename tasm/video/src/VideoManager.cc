#include "VideoManager.h"

#include "TileLocationProvider.h"
#include "TiledVideoManager.h"
#include "ScanOperators.h"
#include "ScanTiledVideoOperator.h"
#include "DecodeOperators.h"
#include "TileOperators.h"
#include "Video.h"
#include "VideoConfiguration.h"

namespace tasm {

void VideoManager::createCatalogIfNecessary() {
    if (!std::filesystem::exists(CatalogPath))
        std::filesystem::create_directory(CatalogPath);
}

void VideoManager::store(const std::filesystem::path &path, const std::string &name) {
    std::shared_ptr<Video> video(new Video(path));

    std::shared_ptr<ScanFileDecodeReader> scan(new ScanFileDecodeReader(video));
    std::shared_ptr<GPUDecodeFromCPU> decode(new GPUDecodeFromCPU(scan, video->configuration(), gpuContext_, lock_));

    auto tileConfigurationProvider = std::make_shared<SingleTileConfigurationProvider>(
            video->configuration().displayWidth,
            video->configuration().displayHeight);

    TileOperator tile(video, decode, tileConfigurationProvider, name, video->configuration().frameRate, gpuContext_, lock_);
    while (!tile.isComplete()) {
        tile.next();
    }
}

void VideoManager::select(const std::string &video,
        std::shared_ptr<MetadataSelection> metadataSelection,
        std::shared_ptr<TemporalSelection> temporalSelection,
        std::shared_ptr<SemanticIndex> semanticIndex) {
    std::shared_ptr<TiledEntry> entry(new TiledEntry(video));

    // Set up scan-multi-tile.
    std::shared_ptr<TiledVideoManager> tiledVideoManager(new TiledVideoManager(entry));
    auto tileLocationProvider = std::make_shared<SingleTileLocationProvider>(tiledVideoManager);
    auto semanticDataManager = std::make_shared<SemanticDataManager>(semanticIndex, entry->name(), metadataSelection, temporalSelection);
    auto scan = std::make_shared<ScanTiledVideoOperator>(entry, semanticDataManager, tileLocationProvider);

    // Set up decode. Specify largest tile dimensions.
    auto maxWidth = tiledVideoManager->largestWidth();
    auto maxHeight = tiledVideoManager->largestHeight();
    auto configuration = video::GetConfiguration(tileLocationProvider->locationOfTileForFrame(0, 0));
    configuration->maxWidth = std::max(maxWidth, configuration->maxWidth);
    configuration->maxHeight = std::max(maxHeight, configuration->maxHeight);
    std::shared_ptr<GPUDecodeFromCPU> decode(new GPUDecodeFromCPU(scan, *configuration, gpuContext_, lock_, maxWidth, maxHeight));

    while (!decode->isComplete())
        decode->next();
}

} // namespace tasm