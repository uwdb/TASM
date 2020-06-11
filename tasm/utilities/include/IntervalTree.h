#ifndef TASM_INTERVALTREE_H
#define TASM_INTERVALTREE_H

#include <algorithm>
#include <vector>
#include <unordered_map>

namespace tasm {

template <typename T>
class IntervalEntry {
public:
    IntervalEntry() {}
    IntervalEntry(T l, T r, int id)
            : l_(l), r_(r), id_(id) {}

    const T &l() const { return l_; }
    const T &r() const { return r_; }
    const int &id() const { return id_; }
private:
    T l_;
    T r_;
    int id_;
};

// TODO: Won't work for floating point intervals because doesn't use epsilon=.
template <typename T>
class IntervalTree {
public:
    IntervalTree() {}

    IntervalTree(T lowerBound, T upperBound, std::vector<IntervalEntry<T>> &intervals)
            : id_(0),
              lowerBound_(lowerBound),
              upperBound_(upperBound)
    {
        root_ = build(lowerBound_, upperBound_, intervals);
    }

    void query(T queryPoint, std::vector<IntervalEntry<T>> &results, int startingNode = 0) const {
        auto center = nodeToCenterPoint_.at(startingNode);
        auto &overlappingByAscendingLeft = nodeToSortedLeft_.at(startingNode);
        if (queryPoint == center) {
            // The query point is the center, so add all of the intervals that intersect the center node.
            results.insert(results.end(), overlappingByAscendingLeft.begin(), overlappingByAscendingLeft.end());
        } else if (queryPoint < center) {
            // Look at intervals in the left subtree.
            // First find the intervals that overlap the center point and also overlap the query point.
            // it points to the first interval that starts after the query point, and therefor no following intervals could
            // cover the query point.
            auto it = std::find_if(overlappingByAscendingLeft.begin(), overlappingByAscendingLeft.end(), [&](const IntervalEntry<T> &entry) {
                return entry.l() > queryPoint;
            });
            results.insert(results.end(), overlappingByAscendingLeft.begin(), it);
            auto leftChild = nodeToLeftChild_.at(startingNode);
            if (leftChild != -1)
                query(queryPoint, results, leftChild);
        } else {
            // Find the first interval that ends before the query point.
            auto &overlappingByDescendingRight = nodeToSortedRight_.at(startingNode);
            auto it = std::find_if(overlappingByDescendingRight.begin(), overlappingByDescendingRight.end(), [&](const IntervalEntry<T> &entry) {
                return entry.r() < queryPoint;
            });
            results.insert(results.end(), overlappingByDescendingRight.begin(), it);
            auto rightChild = nodeToRightChild_.at(startingNode);
            if (rightChild != -1)
                query(queryPoint, results, rightChild);
        }
    }

private:
    int build(T lowerBound, T upperBound, std::vector<IntervalEntry<T>> &intervals) {
        int id = id_++;
        T center = lowerBound + (upperBound - lowerBound) / 2;

        std::vector<IntervalEntry<T>> intervalsToLeft;
        T lowerBoundToLeft = center;
        T upperBoundToLeft = lowerBound;
        std::vector<IntervalEntry<T>> intervalsToRight;
        T lowerBoundToRight = upperBound;
        T upperBoundToRight = center;
        std::vector<IntervalEntry<T>> intervalsIntersectingCenter;

        for (auto &interval : intervals) {
            if (interval.l() > center) {
                intervalsToRight.push_back(interval);
                lowerBoundToRight = std::min(interval.l(), lowerBoundToRight);
                upperBoundToRight = std::max(interval.r(), upperBoundToRight);
            } else if (interval.r() < center) {
                intervalsToLeft.push_back(interval);
                lowerBoundToLeft = std::min(interval.l(), lowerBoundToLeft);
                upperBoundToLeft = std::max(interval.r(), upperBoundToLeft);
            } else {
                intervalsIntersectingCenter.push_back(interval);
            }
        }

        nodeToCenterPoint_[id] = center;

        // Sort left children by ascending starting point.
        std::sort(intervalsIntersectingCenter.begin(), intervalsIntersectingCenter.end(),
                  [](const IntervalEntry<T> &first, const IntervalEntry<T> &second) {
                      return first.l() < second.l();
                  });
        nodeToSortedLeft_[id] = intervalsIntersectingCenter;

        // Sort right children by descending starting point.
        std::sort(intervalsIntersectingCenter.begin(), intervalsIntersectingCenter.end(),
                  [](const IntervalEntry<T> &first, const IntervalEntry<T> &second) {
                      return first.r() > second.r();
                  });
        nodeToSortedRight_[id] = intervalsIntersectingCenter;

        // Set up children.
        nodeToLeftChild_[id] = intervalsToLeft.size() ? build(lowerBoundToLeft, upperBoundToLeft, intervalsToLeft) : -1;
        nodeToRightChild_[id] = intervalsToRight.size() ? build(lowerBoundToRight, upperBoundToRight, intervalsToRight) : -1;

        return id;
    }

    int id_;
    int root_;
    T lowerBound_;
    T upperBound_;
    std::unordered_map<int, int> nodeToLeftChild_;
    std::unordered_map<int, int> nodeToRightChild_;
    std::unordered_map<int, T> nodeToCenterPoint_;
    std::unordered_map<int, std::vector<IntervalEntry<T>>> nodeToSortedLeft_, nodeToSortedRight_;
};

} // namespace tasm

#endif //TASM_INTERVALTREE_H
