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
// TODO: This will likely have to know what pixels on each frame are required to make a good decision.
// TODO: Support including information about the desired resolution for each tile.
// TODO: This should be split into 2 classes tileLayoutForFrame() and maximumNumberOfTiles().
class TileConfigurationProvider {
public:
    virtual unsigned int maximumNumberOfTiles() = 0;
    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) = 0;

    virtual ~TileConfigurationProvider() { }
};

class KeyframeConfigurationProvider {
public:
    virtual bool shouldFrameBeKeyframe(unsigned int frame) = 0;

    virtual ~KeyframeConfigurationProvider() { }
};

class MetadataTileConfigurationProvider : public TileConfigurationProvider, public KeyframeConfigurationProvider {
public:
    // Init with metadata specification.
    // Also have to be able to specify where keyframes should be.
    // We may be only cracking a particular range of frames, or further cracking an existing tile.
    MetadataTileConfigurationProvider(std::shared_ptr<metadata::MetadataManager> &metadataManager,
            unsigned int firstFrameToConsider,
            unsigned int lastFrameToConsider,
            Rectangle portionOfFrameToConsider)
        : metadataManager_(metadataManager),
        firstFrameToConsider_(firstFrameToConsider),
        lastFrameToConsider_(lastFrameToConsider),
        portionOfFrameToConsider_(std::move(portionOfFrameToConsider))
    {
        pickKeyframes();
        computeTileConfigurations();
    }

    unsigned int maximumNumberOfTiles() override {
        return 0;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        return EmptyTileLayout;
    }

    bool shouldFrameBeKeyframe(unsigned int frame) override {
        return false;
    }

private:
    void computeTileConfigurations();
    void pickKeyframes();
    void setUpGOPs();
    std::vector<int> framesForGOP(unsigned int gopNumber);

    std::shared_ptr<metadata::MetadataManager> metadataManager_;
    unsigned int firstFrameToConsider_;
    unsigned int lastFrameToConsider_;
    Rectangle portionOfFrameToConsider_;
    std::set<int> keyframes_;
    std::vector<std::vector<int>> gopFramesThatContainObject_;
};

class IdealTileConfigurationProvider : public TileConfigurationProvider {
public:
    IdealTileConfigurationProvider(std::shared_ptr<metadata::MetadataManager> metadataManager,
            unsigned int frameWidth,
            unsigned int frameHeight)
        : metadataManager_(metadataManager),
        frameWidth_(frameWidth),
        frameHeight_(frameHeight)
    { }

    unsigned int maximumNumberOfTiles() override {
        return 0;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override;

private:
    std::shared_ptr<metadata::MetadataManager> metadataManager_;
    unsigned int frameWidth_;
    unsigned int frameHeight_;

    std::unordered_map<unsigned int, TileLayout> frameToTileLayout_;
};

class NaiveTileConfigurationProvider : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 2;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout tileLayout = TileLayout(2, 1, {480, 480}, {576});
        return tileLayout;
    }
};

class GOP30TileConfigurationProvider : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 4;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout twoTileLayout = TileLayout(2, 1, {480, 480}, {576});
        static TileLayout fourTileLayout = TileLayout(2, 2, {480, 480}, {288, 288});

        if (frame < 30)
            return twoTileLayout;
        else if (30 <= frame && frame < 60)
            return fourTileLayout;
        else if (60 <= frame && frame < 90)
            return twoTileLayout;
        else if (frame >= 90)
            return fourTileLayout;
        else
            return twoTileLayout;
    }
};

class SingleTileFor2kConfigurationProvider: public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 1;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout oneTileLayout = TileLayout(1, 1, {1920}, {1080});
        return oneTileLayout;
    }
};

class Threex3TileFor1kConfigurationProvider: public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 9;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout threex3Layout = TileLayout(3, 3, {320, 320, 320}, {180, 180, 180});
        return threex3Layout;
    }
};

class Threex3TileFor2kConfigurationProvider: public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 9;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout threex3Layout = TileLayout(3, 3, {640, 640, 640}, {360, 360, 360});
        return threex3Layout;
    }
};

class Threex3TileFor4kConfigurationProvider: public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 9;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout threex3Layout = TileLayout(3, 3, {1280, 1280, 1280}, {660, 660, 660});
        return threex3Layout;
    }
};


class SingleTileFor4kConfigurationProvider: public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 1;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout oneTileLayout = TileLayout(1, 1, {3840}, {1980});
        return oneTileLayout;
    }
};

class AlternatingTileFor4kConfigurationProvider: public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 1;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout oneTileLayout = TileLayout(1, 1, {3840}, {1980});
        static TileLayout twoTileLayout = TileLayout(2, 2, {1920, 1920}, {1080, 900});

        return ((frame / 30) % 2) ? oneTileLayout : twoTileLayout;
    }
};

class GOP30TileConfigurationProvider2x2And3x3 : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 9;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
//        static TileLayout fourTileLayout = TileLayout(2, 2, {960, 960}, {540, 540});
        static TileLayout nineTileLayout = TileLayout(3, 3, {640, 640, 640}, {360, 360, 360});

        return nineTileLayout;
//        if ((frame / 30) % 30)
//            return fourTileLayout;
//        else
//            return nineTileLayout;
    }

};

class CustomVanTileConfigurationProvider : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 4;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout twoTileLayout = TileLayout(2, 1, {480, 480}, {576});
        static TileLayout fourTileLayout = TileLayout(2, 2, {480, 480}, {288, 288});

        if (frame <= 15)
            return twoTileLayout;
        else if (frame > 15 && frame < 55)
            return fourTileLayout;
        else
            return twoTileLayout;
    }
};

class CustomVan4x4TileConfigurationProvider : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 4;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override {
        static TileLayout twoTileLayout = TileLayout(2, 1, {480, 480}, {576});
        static TileLayout fourTileLayout = TileLayout(2, 2, {480, 480}, {288, 288});

        if (frame <= 15)
            return fourTileLayout;
        else if (frame > 15 && frame < 55)
            return twoTileLayout;
        else
            return fourTileLayout;
    }
};

class CustomJacksonSquareTileConfigurationProvider : public TileConfigurationProvider {
public:
    unsigned int maximumNumberOfTiles() override {
        return 0;
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) override;

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
        totalHeight_(0)
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

private:
    void loadAllTileConfigurations();

    const catalog::MultiTileEntry entry_;

    IntervalTree<unsigned int> intervalTree_;
    std::unordered_map<int, std::filesystem::path> directoryIdToTileDirectory_;
    std::unordered_map<int, std::shared_ptr<const TileLayout>> directoryIdToTileLayout_;
    std::unordered_map<TileLayout, std::shared_ptr<const TileLayout>> tileLayoutReferences_;

//    std::map<lightdb::Interval<unsigned int>, std::list<TileLayout>> intervalToAvailableTileLayouts_;
//    std::map<unsigned int, lightdb::CoalescedIntervals<unsigned int>, std::greater<unsigned int>> numberOfTilesToFrameIntervals_;
//    std::map<lightdb::Interval<unsigned int>, std::filesystem::path> intervalToTileDirectory_;
//    std::unordered_map<std::string, TileLayout> tileDirectoryToLayout_;
    mutable std::mutex mutex_;

    unsigned int totalWidth_;
    unsigned int totalHeight_;
};

class TileLocationProvider {
public:
    virtual std::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const = 0;
    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) const = 0;
    unsigned int frameOffsetInTileFile(const std::filesystem::path &tilePath) const {
        return catalog::TileFiles::firstAndLastFramesFromPath(tilePath.parent_path()).first;
    }

    virtual ~TileLocationProvider() { }
};

//class MultiTileLocationProvider : public TileLocationProvider {
//public:
//    MultiTileLocationProvider(std::shared_ptr<const TileLayoutsManager> tileLayoutsManager,
//            std::shared_ptr<const metadata::MetadataManager> metadataManager,
//            unsigned int tileGroupSize = 30)
//        : tileLayoutsManager_(tileLayoutsManager),
//          metadataManager_(metadataManager),
//          tileGroupSize_(tileGroupSize)
//    { }
//
//    std::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const override {
//        return tileLayoutsManager_->locationOfTileForFrameAndConfiguration(tileNumber, frame, tileLayoutForFrame(frame));
//    }
//
//    const TileLayout &tileLayoutForFrame(unsigned int frame) const override;
//
//
//private:
//    void insertTileLayoutForTileGroup(TileLayout &layout, unsigned int frame) const;
//    unsigned int tileGroupForFrame(unsigned int frame) const {
//        return frame / tileGroupSize_;
//    }
//
//    std::shared_ptr<const TileLayoutsManager> tileLayoutsManager_;
//    std::shared_ptr<const metadata::MetadataManager> metadataManager_;
//    unsigned int tileGroupSize_;
//
//    mutable std::unordered_map<TileLayout, std::shared_ptr<const TileLayout>> tileLayoutReferences_;
//    mutable std::unordered_map<unsigned int, std::shared_ptr<const TileLayout>> tileGroupToTileLayout_;
//    mutable std::recursive_mutex mutex_;
//};

class SingleTileLocationProvider : public TileLocationProvider {
public:
    SingleTileLocationProvider(std::shared_ptr<const TileLayoutsManager> tileLayoutsManager)
        : tileLayoutsManager_(tileLayoutsManager)
    { }

    std::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const override {
        return tileLayoutsManager_->locationOfTileForId(tileNumber, layoutIdForFrame(frame));
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) const override {
        std::scoped_lock lock(mutex_);
        return tileLayoutsManager_->tileLayoutForId(layoutIdForFrame(frame));
    }

private:
    int layoutIdForFrame(unsigned int frame) const {
        std::scoped_lock lock(mutex_);

        if (frameToLayoutId_.count(frame))
            return frameToLayoutId_.at(frame);

        auto layoutIds = tileLayoutsManager_->tileLayoutIdsForFrame(frame);
        assert(layoutIds.size() == 1);

        frameToLayoutId_[frame] = layoutIds.front();
        return layoutIds.front();
    }

    std::shared_ptr<const TileLayoutsManager> tileLayoutsManager_;
    mutable std::unordered_map<unsigned int, int> frameToLayoutId_;
    mutable std::recursive_mutex mutex_;
};

} // namespace lightdb::tiles

#endif //LIGHTDB_TILECONFIGURATIONPROVIDER_H
