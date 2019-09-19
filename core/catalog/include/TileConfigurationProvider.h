#ifndef LIGHTDB_TILECONFIGURATIONPROVIDER_H
#define LIGHTDB_TILECONFIGURATIONPROVIDER_H

#include "Catalog.h"
#include "Files.h"
#include "Metadata.h"
#include "TileLayout.h"
#include <boost/functional/hash.hpp>
#include <MetadataLightField.h>

namespace lightdb {
class Interval {
public:
    Interval(unsigned int start, unsigned int end)
            : start_(start), end_(end) {}

    Interval(std::pair<unsigned int, unsigned int> startAndEnd)
        : start_(startAndEnd.first),
        end_(startAndEnd.second)
    { }

    Interval(const Interval &other) = default;

    Interval(const Interval &first, const Interval &second)
        : Interval(first)
    {
        expandToInclude(second);
    }

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

    unsigned int start() const { return start_; }

    unsigned int end() const { return end_; }

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
    unsigned int start_;
    unsigned int end_;
};

class CoalescedIntervals {
public:
    CoalescedIntervals()
        : min_(UINT32_MAX),
        max_(0)
    { }

    const std::set<Interval> &intervals() const {
        return intervals_;
    }

    void addInterval(const Interval &newInterval) {
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

    bool contains(unsigned int value) const {
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

    Interval extents() const {
        return Interval(min_, max_);
    }

private:
    unsigned int min_;
    unsigned int max_;
    std::set<Interval> intervals_;
};


} // namespace lightdb

namespace std {
template<>
struct hash<lightdb::Interval> {
    size_t operator()(const lightdb::Interval &interval) const {
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

// TODO: Should this have a TileConfigurationProvider that uses the available possible tile layouts
// to return one for each frame?
class TileLayoutsManager {
public:
    TileLayoutsManager(catalog::MultiTileEntry entry)
        : entry_(std::move(entry))
    {
        loadAllTileConfigurations();
    }

    const catalog::MultiTileEntry &entry() const { return entry_; }

    std::list<TileLayout> tileLayoutsForFrame(unsigned int frameNumber) const;
    unsigned int maximumNumberOfTilesForFrames(const std::vector<int> &frames) const;
    std::filesystem::path locationOfTileForFrameAndConfiguration(unsigned int tileNumber, unsigned int frame, const tiles::TileLayout &tileLayout) const;

private:
    void loadAllTileConfigurations();

    const catalog::MultiTileEntry entry_;

    std::map<lightdb::Interval, std::list<TileLayout>> intervalToAvailableTileLayouts_;
    std::map<unsigned int, lightdb::CoalescedIntervals, std::greater<unsigned int>> numberOfTilesToFrameIntervals_;
    std::map<lightdb::Interval, std::filesystem::path> intervalToTileDirectory_;
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

class SingleTileLocationProvider : public TileLocationProvider {
public:
    SingleTileLocationProvider(std::shared_ptr<const TileLayoutsManager> tileLayoutsManager)
        : tileLayoutsManager_(tileLayoutsManager)
    { }

    std::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const override {
        return tileLayoutsManager_->locationOfTileForFrameAndConfiguration(tileNumber, frame, tileLayoutForFrame(frame));
    }

    const TileLayout &tileLayoutForFrame(unsigned int frame) const override {
        if (frameToTileLayout_.count(frame))
            return *frameToTileLayout_.at(frame);

        auto layouts = tileLayoutsManager_->tileLayoutsForFrame(frame);
        assert(layouts.size() == 1);
        auto &layout = layouts.front();
        if (!tileLayoutReferences_.count(layout))
            tileLayoutReferences_[layout] = std::make_shared<const TileLayout>(layout);

        frameToTileLayout_[frame] = tileLayoutReferences_.at(layout);
        return *frameToTileLayout_.at(frame);
    }

private:
    std::shared_ptr<const TileLayoutsManager> tileLayoutsManager_;
    mutable std::unordered_map<TileLayout, std::shared_ptr<const TileLayout>> tileLayoutReferences_;
    // This should probably be a LRU cache to avoid it growing extremely large for a long video.
    mutable std::unordered_map<unsigned int, std::shared_ptr<const TileLayout>> frameToTileLayout_;
};

} // namespace lightdb::tiles

#endif //LIGHTDB_TILECONFIGURATIONPROVIDER_H
