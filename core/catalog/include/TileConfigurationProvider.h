#ifndef LIGHTDB_TILECONFIGURATIONPROVIDER_H
#define LIGHTDB_TILECONFIGURATIONPROVIDER_H

#include "Catalog.h"
#include "Files.h"
#include "TileLayout.h"
#include <boost/functional/hash.hpp>

namespace lightdb {
class Interval {
public:
    Interval(unsigned int start, unsigned int end)
            : start_(start), end_(end) {}

    Interval(std::pair<unsigned int, unsigned int> startAndEnd)
        : start_(startAndEnd.first),
        end_(startAndEnd.second)
    { }

    unsigned int start() const { return start_; }

    unsigned int end() const { return end_; }

    bool contains(unsigned int value) const {
        return value >= start_ && value <= end_;
    }

    bool operator==(const Interval &other) const {
        return start_ == other.start_
               && end_ == other.end_;
    }

    bool intersects(const Interval &other) const {
        return !(other.start() > end_ || start_ > other.end());
    }

    void expandToInclude(const Interval &other) {
        start_ = std::min(start_, other.start());
        end_ = std::max(end_, other.end());
    }

    static bool compare(const Interval &lhs, const Interval &rhs) {
        return lhs.start() < rhs.start();
    }

private:
    unsigned int start_;
    unsigned int end_;
};

class CoalescedIntervals {
public:
    void addInterval(const Interval &newInterval) {
        if (newInterval.start() < min_)
            min_ = newInterval.start();

        if (newInterval.end() > max_)
            max_ = newInterval.end();

        for (auto &interval : intervals_) {
            if (newInterval.intersects(interval)) {
                interval.expandToInclude(newInterval);
                return;
            }
        }

        intervals_.push_back(newInterval);
    }

    bool contains(unsigned int value) const {
        for (const auto &interval : intervals_) {
            if (interval.contains(value))
                return true;
        }
        return false;
    }

    Interval extents() const {
        return Interval(min_, max_);
    }

private:
    std::list<Interval> intervals_;
    unsigned int min_;
    unsigned int max_;
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

    std::unordered_map<lightdb::Interval, std::list<TileLayout>> intervalToAvailableTileLayouts_;
    std::map<unsigned int, lightdb::CoalescedIntervals, std::greater<unsigned int>> numberOfTilesToFrameIntervals_;
    std::unordered_map<lightdb::Interval, std::filesystem::path> intervalToTileDirectory_;
};

class TileLocationProvider {
public:
    virtual std::filesystem::path locationOfTileForFrame(unsigned int tileNumber, unsigned int frame) const = 0;
    virtual const TileLayout &tileLayoutForFrame(unsigned int frame) const = 0;
    unsigned int frameOffsetInTileFile(const std::filesystem::path &tilePath) const {
        return catalog::TileFiles::firstAndLastFramesFromPath(tilePath.parent_path()).first;
    }
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
