//
// Created by Maureen Daum on 2019-06-28.
//

#ifndef LIGHTDB_TIMER_H
#define LIGHTDB_TIMER_H

#include <chrono>
#include <iostream>
#include "StatsCollector.h"

namespace lightdb {
class Timer {
public:
    Timer()
        : currentOperation_("")
    { }

    void startSection(const std::string &id) {
//        return; // Weird when have multiple operators with same id.
        // If we're starting a section for an operator, we shouldn't already have that operator waiting to finish.
        assert(std::find(operatorsInProgress_.begin(), operatorsInProgress_.end(), id) == operatorsInProgress_.end());
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

        if (currentOperation_.length()) {
            operatorToExecutionTime_[currentOperation_] += (now - start_);
            operatorsInProgress_.push_back(currentOperation_);
        }

        currentOperation_ = id;
        start_ = now;
    }

    void endSection(const std::string &id) {
//        return;
        assert(currentOperation_ == id);

        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        operatorToExecutionTime_[currentOperation_] += (now - start_);

        currentOperation_ = "";

        if (operatorsInProgress_.size()) {
            currentOperation_ = operatorsInProgress_.back();
            operatorsInProgress_.pop_back();
            start_ = now;
        }
    }

    double totalTimeInMillis(const std::string &id) {
        return toMillis_(operatorToExecutionTime_[id]);
    }

    void printAllTimes() const {
        assert(!currentOperation_.length());
        assert(!operatorsInProgress_.size());
        for (auto it = operatorToExecutionTime_.begin(); it != operatorToExecutionTime_.end(); it++) {
            std::cout << "ANALYSIS " << it->first << " took " << toMillis_(it->second) << " ms" << std::endl;
        }
    }

    void shareWithStatsCollector() const {
        for (auto it = operatorToExecutionTime_.begin(); it != operatorToExecutionTime_.end(); it++)
            StatsCollector::instance().addRuntime(it->first, toMillis_(it->second));
    }

    void reset() {
        currentOperation_.clear();
        operatorsInProgress_.clear();
        operatorToExecutionTime_.clear();
    }
private:
    double toMillis_(std::chrono::system_clock::duration duration) const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    std::chrono::system_clock::time_point start_;

    std::string currentOperation_;
    std::vector<std::string> operatorsInProgress_;
    std::unordered_map<std::string, std::chrono::system_clock::duration> operatorToExecutionTime_;
};

static Timer GLOBAL_TIMER{};

static Timer RECONFIGURE_DECODER_TIMER{};
static Timer READ_FROM_NEW_FILE_TIMER{};
} // namespace lightdb

#endif //LIGHTDB_TIMER_H
