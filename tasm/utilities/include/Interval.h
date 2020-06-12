#ifndef TASM_INTERVAL_H
#define TASM_INTERVAL_H

#include <boost/functional/hash.hpp>
#include <set>

namespace tasm::interval{

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

} // namespace tasm::interval

namespace std {
template <typename T>
struct hash<tasm::interval::Interval<T>> {
size_t operator()(const tasm::interval::Interval<T> &interval) const {
    size_t seed = 0;
    boost::hash_combine(seed, interval.start());
    boost::hash_combine(seed, interval.end());

    return seed;
}
};
} // namespace std

#endif //TASM_INTERVAL_H
