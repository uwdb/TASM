//
// Created by Maureen Daum on 2019-06-28.
//

#ifndef LIGHTDB_TIMER_H
#define LIGHTDB_TIMER_H

namespace lightdb {
class Timer {
public:
    Timer()
        : executionTime_(0)
    { }

    void startSection() {
        start_ = std::chrono::system_clock::now();
    }

    void endSection() {
        executionTime_ += (std::chrono::system_clock::now() - start_);
    }

    double totalTimeInMillis() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(executionTime_).count();
    }
private:
    std::chrono::system_clock::time_point start_;
    std::chrono::system_clock::duration executionTime_;
};
} // namespace lightdb

#endif //LIGHTDB_TIMER_H
