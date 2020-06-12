#include "VideoManager.h"

#include "ImageUtilities.h"
#include "MergeTiles.h"
#include "TileLocationProvider.h"
#include "TiledVideoManager.h"
#include "ScanOperators.h"
#include "ScanTiledVideoOperator.h"
#include "DecodeOperators.h"
#include "SemanticIndex.h"
#include "SemanticSelection.h"
#include "TemporalSelection.h"
#include "TileOperators.h"
#include "TransformToImage.h"
#include "Video.h"
#include "VideoConfiguration.h"


namespace tasm {

void VideoManager::createCatalogIfNecessary() {
    if (!std::experimental::filesystem::exists(CatalogPath))
        std::experimental::filesystem::create_directory(CatalogPath);
}

void VideoManager::store(const std::experimental::filesystem::path &path, const std::string &name) {
    std::shared_ptr<Video> video(new Video(path));
    auto tileConfigurationProvider = std::make_shared<SingleTileConfigurationProvider>(
            video->configuration().displayWidth,
            video->configuration().displayHeight);
    storeTiledVideo(video, tileConfigurationProvider, name);
}

void VideoManager::storeWithUniformLayout(const std::experimental::filesystem::path &path, const std::string &name, unsigned int numRows, unsigned int numColumns) {
    std::shared_ptr<Video> video(new Video(path));
    auto tileConfigurationProvider = std::make_shared<UniformTileconfigurationProvider>(numRows, numColumns, video->configuration());
    storeTiledVideo(video, tileConfigurationProvider, name);
}

void VideoManager::storeTiledVideo(std::shared_ptr<Video> video, std::shared_ptr<TileLayoutProvider> tileLayoutProvider, const std::string &savedName) {
    std::shared_ptr<ScanFileDecodeReader> scan(new ScanFileDecodeReader(video));
    std::shared_ptr<GPUDecodeFromCPU> decode(new GPUDecodeFromCPU(scan, video->configuration(), gpuContext_, lock_));

    TileOperator tile(video, decode, tileLayoutProvider, savedName, video->configuration().frameRate, gpuContext_, lock_);
    while (!tile.isComplete()) {
        tile.next();
    }
}

std::unique_ptr<ImageIterator> VideoManager::select(const std::string &video,
                                                     std::shared_ptr<MetadataSelection> metadataSelection,
                                                     std::shared_ptr<TemporalSelection> temporalSelection,
                                                     std::shared_ptr<SemanticIndex> semanticIndex) {
    std::shared_ptr<TiledEntry> entry(new TiledEntry(video));

    // Set up scan of a tiled video.
    std::shared_ptr<TiledVideoManager> tiledVideoManager(new TiledVideoManager(entry));
    auto tileLocationProvider = std::make_shared<SingleTileLocationProvider>(tiledVideoManager);
    auto semanticDataManager = std::make_shared<SemanticDataManager>(semanticIndex, entry->name(), metadataSelection, temporalSelection);
    auto scan = std::make_shared<ScanTiledVideoOperator>(entry, semanticDataManager, tileLocationProvider);

    // Set up decode. Specify largest tile dimensions which are required to successfully reconfigure the decoder.
    auto maxWidth = tiledVideoManager->largestWidth();
    auto maxHeight = tiledVideoManager->largestHeight();
    auto configuration = video::GetConfiguration(tileLocationProvider->locationOfTileForFrame(0, 0));
    configuration->maxWidth = std::max(maxWidth, configuration->maxWidth);
    configuration->maxHeight = std::max(maxHeight, configuration->maxHeight);
    std::shared_ptr<GPUDecodeFromCPU> decode(new GPUDecodeFromCPU(scan, *configuration, gpuContext_, lock_, maxWidth, maxHeight));

    // Set up tile merger.
    std::shared_ptr<MergeTilesOperator> merge(new MergeTilesOperator(decode, semanticDataManager, tileLocationProvider));

    // Transform pixels to RGB images.
    std::shared_ptr<TransformToImage> transform(new TransformToImage(merge, maxWidth, maxHeight));

    return std::make_unique<ImageIterator>(transform);
}

} // namespace tasm