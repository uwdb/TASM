#ifndef LIGHTDB_TILECONFIGURATIONPROVIDER_H
#define LIGHTDB_TILECONFIGURATIONPROVIDER_H

#include "Catalog.h"
#include "Files.h"
#include "IntervalTree.h"
#include "Metadata.h"
#include "TileLayout.h"
#include <boost/functional/hash.hpp>
#include <MetadataLightField.h>
#include <mutex>

namespace lightdb {
template <typename T>
class Interval {
public:
    Interval(T start, T end)
            : start_(start), end_(end) {}

    Interval(std::pair<T, T> startAndEnd)
        : start_(startAndEnd.first),
        end_(startAndEnd.second)
    { }

    Interval(const Interval &other) = default;

    Interval(const Interval &first, const Interval &second)
        : Interval(first)
    {
        expandToInclude(second);
    }

    Interval() = default;

    bool operator==(const Interval &other) const {
        return start_ == other.start_
               && end_ == other.end_;
    }

    bool operator<(const Interval &rhs) const {
        if (start_ != rhs.start_)
            return start_ < rhs.start_;
        else
            return end_ < rhs.end_;
    }

    T start() const { return start_; }

    T end() const { return end_; }

    bool contains(unsigned int value) const {
        return value >= start_ && value <= end_;
    }

    bool intersects(const Interval &other) const {
        return !(other.start() > end_ || start_ > other.end());
    }

    void expandToInclude(const Interval &other) {
        start_ = std::min(start_, other.start());
        end_ = std::max(end_, other.end());
    }

private:
    T start_;
    T end_;
};

template <typename T>
class CoalescedIntervals {
public:
    CoalescedIntervals()
        : min_(UINT32_MAX),
        max_(0)
    { }

    const std::set<Interval<T>> &intervals() const {
        return intervals_;
    }

    void addInterval(const Interval<T> &newInterval) {
        if (newInterval.start() < min_)
            min_ = newInterval.start();

        if (newInterval.end() > max_)
            max_ = newInterval.end();

        bool insertedNewInterval = false;
        for (auto intervalIt = intervals_.begin(); intervalIt != intervals_.end(); ++intervalIt) {
            if (newInterval.intersects(*intervalIt)) {
                auto expandedInterval = Interval(newInterval, *intervalIt);
                intervals_.erase(intervalIt);
                intervals_.insert(expandedInterval);

                // Start again to coalesce any intervals that now overlap.
//                intervalIt = intervals_.begin();

                insertedNewInterval = true;
                break; // TODO: Start again to find intervals that may now overlap.
            }
        }
        if (!insertedNewInterval)
            intervals_.insert(newInterval);

    }

    bool contains(T value) const {
        // Use upper_bound to limit comparisons.
        auto it = intervals_.upper_bound(Interval(value, UINT32_MAX));
        if (it == intervals_.end())
            std::advance(it, -1);

        for (; it->end() >= value; --it) {
            if (it->contains(value))
                return true;

            if (it == intervals_.begin())
                break;
        }
        return false;
    }

    Interval<T> extents() const {
        return Interval<T>(min_, max_);
    }

private:
    T min_;
    T max_;
    std::set<Interval<T>> intervals_;
};


} // namespace lightdb

namespace std {
template <typename T>
struct hash<lightdb::Interval<T>> {
    size_t operator()(const lightdb::Interval<T> &interval) const {
        size_t seed = 0;
        boost::hash_combine(seed, interval.start());
        boost::hash_combine(seed, interval.end());

        return seed;
    }
};
} // namespace std

namespace lightdb::tiles {

class TileLayoutProvider {
public:
    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) = 0;
    ~TileLayoutProvider() {}
};

class TileConfigurationProvider : public TileLayoutProvider {
public:
    virtual unsigned int maximumNumberOfTiles() = 0;
//    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) = 0;

    virtual ~TileConfigurationProvider() { }
};

class ConglomerationTileConfigurationProvider : public TileConfigurationProvider {
public:
    ConglomerationTileConfigurationProvider(std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<TileConfigurationProvider>>> gopToConfigProvider, unsigned int gopLength)
        : gopToConfigProvider_(std::move(gopToConfigProvider)),
        gopLength_(gopLength)
    {}

    unsigned int maximumNumberOfTiles() override { return 0; }
    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        return gopToConfigProvider_->at(frame / gopLength_)->tileLayoutForFrame(frame);
    }

private:
    std::unique_ptr<std::unordered_map<unsigned int, std::shared_ptr<TileConfigurationProvider>>> gopToConfigProvider_;
    unsigned int gopLength_;
};

class KeyframeConfigurationProvider {
public:
    virtual bool shouldFrameBeKeyframe(unsigned int frame) = 0;

    virtual ~KeyframeConfigurationProvider() { }
};

class SingleTileConfigurationProvider: public TileConfigurationProvider {
public:
    SingleTileConfigurationProvider(unsigned int totalWidth, unsigned int totalHeight)
        : totalWidth_(totalWidth), totalHeight_(totalHeight), layout_(1, 1, {totalWidth_}, {totalHeight_})
    { }

    unsigned int maximumNumberOfTiles() override {
        return 1;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        return layout_;
    }

private:
    unsigned int totalWidth_;
    unsigned  int totalHeight_;
    TileLayout layout_;
};

template <int Rows, int Columns>
class UniformTileconfigurationProvider: public TileConfigurationProvider {
public:
    UniformTileconfigurationProvider(unsigned int totalWidth, unsigned int totalHeight)
        : widthPerColumn_(totalWidth / Columns),
        heightPerRow_(totalHeight / Rows)
    {}

    unsigned int maximumNumberOfTiles() override {
        return Columns * Rows;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        if (layoutPtr)
            return *layoutPtr;

        layoutPtr = std::make_unique<TileLayout>(Columns, Rows, std::vector<unsigned int>(Columns, widthPerColumn_), std::vector<unsigned int>(Rows, heightPerRow_));
        return *layoutPtr;
    }

private:
    std::vector<unsigned int> tile_dimensions(unsigned int codedDimension, unsigned int displayDimension, unsigned int numTiles) {
        static unsigned int CTBS_SIZE_Y = 32;
        std::vector<unsigned int> dimensions(numTiles);
        unsigned int total = 0;
        for (auto i = 0u; i < numTiles; ++i) {
            unsigned int proposedDimension = ((i + 1) * codedDimension / numTiles) - (i * codedDimension / numTiles);
            if (total + proposedDimension > displayDimension)
                proposedDimension = displayDimension - total;

            dimensions[i] = proposedDimension;
            total += proposedDimension;
        }
        return dimensions;
    }

    unsigned int widthPerColumn_;
    unsigned int heightPerRow_;
    std::unique_ptr<TileLayout> layoutPtr;
};

class GroupingExtentsTileConfigurationProvider : public TileConfigurationProvider {
public:
    GroupingExtentsTileConfigurationProvider(unsigned int tileLayoutDuration,
                                                std::shared_ptr<const metadata::MetadataManager> metadataManager,
                                                unsigned int frameWidth,
                                                unsigned int frameHeight,
                                                bool shouldConsiderAllObjectsInFrames = false)
        : tileLayoutDuration_(tileLayoutDuration),
        metadataManager_(metadataManager),
        frameWidth_(frameWidth),
        frameHeight_(frameHeight),
        shouldConsiderAllObjectsInFrames_(shouldConsiderAllObjectsInFrames)
    { }

    unsigned int maximumNumberOfTiles() override { return 0; }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override;

private:
    std::vector<unsigned int> tileDimensions(Interval<int> interval, int minDistance, int totalDimension);

    unsigned int tileLayoutDuration_;
    std::shared_ptr<const metadata::MetadataManager> metadataManager_;
    unsigned int frameWidth_;
    unsigned int frameHeight_;
    bool shouldConsiderAllObjectsInFrames_;
    std::unordered_map<unsigned int, TileLayout> layoutIntervalToTileLayout_;
};

class GroupingTileConfigurationProvider : public TileConfigurationProvider {
public:
    GroupingTileConfigurationProvider(unsigned int tileGroupDuration,
                                        std::shared_ptr<const metadata::MetadataManager> metadataManager,
                                        unsigned int frameWidth,
                                        unsigned int frameHeight)
        : tileGroupDuration_(tileGroupDuration),
          metadataManager_(metadataManager),
          frameWidth_(frameWidth),
          frameHeight_(frameHeight)
    { }

    unsigned int maximumNumberOfTiles() override {
        return 0;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override;

private:
    std::vector<unsigned int> tileDimensions(const std::vector<Interval<int>> &sortedIntervals, int minDistance, int totalDimension);

    unsigned int tileGroupDuration_;
    std::shared_ptr<const metadata::MetadataManager> metadataManager_;
    unsigned int frameWidth_;
    unsigned int frameHeight_;
    std::unordered_map<unsigned int, TileLayout> tileGroupToTileLayout_;
};

// TODO: Should this have a TileConfigurationProvider that uses the available possible tile layouts
// to return one for each frame?
class TileLayoutsManager {
public:
    TileLayoutsManager(catalog::MultiTileEntry entry)
        : entry_(std::move(entry)),
        totalWidth_(0),
        totalHeight_(0),
        largestWidth_(0),
        largestHeight_(0),
        maximumFrame_(0),
        hasASingleTile_(false)
    {
        loadAllTileConfigurations();
    }

    const catalog::MultiTileEntry &entry() const { return entry_; }

    std::vector<int> tileLayoutIdsForFrame(unsigned int frameNumber) const;
    const TileLayout &tileLayoutForId(int id) const { return *directoryIdToTileLayout_.at(id); }
    std::filesystem::path locationOfTileForId(unsigned int tileNumber, int id) const;
//    unsigned int maximumNumberOfTilesForFrames(const std::vector<int> &frames) const;
//    std::filesystem::path locationOfTileForFrameAndConfiguration(unsigned int tileNumber, unsigned int frame, const tiles::TileLayout &tileLayout) const;

    unsigned int totalWidth() const { return totalWidth_; }
    unsigned int totalHeight() const { return totalHeight_; }

    unsigned int largestWidth() const { return largestWidth_; }
    unsigned int largestHeight() const { return largestHeight_; }

    unsigned int maximumFrame() const { return maximumFrame_; }

    bool hasASingleTile() const { return hasASingleTile_; }

private:
    void loadAllTileConfigurations();

    const catalog::MultiTileEntry entry_;

    IntervalTree<unsigned int> intervalTree_;

public: // For sake of measuring.
    std::unordered_map<int, std::filesystem::path> directoryIdToTileDirectory_;
    std::unordered_map<int, std::shared_ptr<const TileLayout>> directoryIdToTileLayout_;
    std::unordered_map<TileLayout, std::shared_ptr<const TileLayout>> tileLayoutReferences_;

private:

//    std::map<lightdb::Interval<unsigned int>, std::list<TileLayout>> intervalToAvailableTileLayouts_;
//    std::map<unsigned int, lightdb::CoalescedIntervals<unsigned int>, std::greater<unsigned int>> numberOfTilesToFrameIntervals_;
//    std::map<lightdb::Interval<unsigned int>, std::filesystem::path> intervalToTileDirectory_;
//    std::unordered_map<std::string, TileLayout> tileDirectoryToLayout_;
    mutable std::mutex mutex_;

    unsigned int totalWidth_;
    unsigned int totalHeight_;
    unsigned int largestWidth_;
    unsigned int largestHeight_;
    unsigned int maximumFrame_;
    bool hasASingleTile_;
};

class TileLocationProvider : public TileLayoutProvider {
public:
    virtual std::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const = 0;
//    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) const = 0;
    unsigned int frameOffsetInTileFile(const std::filesystem::path &tilePath) const {
        return catalog::TileFiles::firstAndLastFramesFromPath(tilePath.parent_path()).first;
    }
    virtual unsigned int lastFrameWithLayout() const = 0;

    virtual ~TileLocationProvider() { }
};

class SingleTileLocationProvider : public TileLocationProvider {
public:
    SingleTileLocationProvider(std::shared_ptr<const TileLayoutsManager> tileLayoutsManager)
        : tileLayoutsManager_(tileLayoutsManager)
    { }

    std::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const override {
        return tileLayoutsManager_->locationOfTileForId(tileNumber, layoutIdForFrame(frame));
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        std::scoped_lock lock(mutex_);
        return tileLayoutsManager_->tileLayoutForId(layoutIdForFrame(frame));
    }

    unsigned int lastFrameWithLayout() const override {
        return tileLayoutsManager_->maximumFrame();
    }

private:
    int layoutIdForFrame(unsigned int frame) const {
        std::scoped_lock lock(mutex_);

        if (frameToLayoutId_.count(frame))
            return frameToLayoutId_.at(frame);

        auto layoutIds = tileLayoutsManager_->tileLayoutIdsForFrame(frame);
//        assert(layoutIds.size() == 1);
        // Pick the one with the largest value.
        auto layoutId = *std::max_element(layoutIds.begin(), layoutIds.end());

        frameToLayoutId_[frame] = layoutId;
        return layoutId;
    }

    std::shared_ptr<const TileLayoutsManager> tileLayoutsManager_;
    mutable std::unordered_map<unsigned int, int> frameToLayoutId_;
    mutable std::recursive_mutex mutex_;
};

} // namespace lightdb::tiles

#endif //LIGHTDB_TILECONFIGURATIONPROVIDER_H
