#pragma once
#include "mcr/inference/constraint.hpp"
#include <deque>

namespace mcr::detail {
class WorkQueue {
public:
    explicit WorkQueue(const Problem& problem) : problem_(problem),queued_(problem.constraints().size(),true) {
        for(std::size_t r=0;r<queued_.size();++r) queue_.push_back(r);
    }
    [[nodiscard]] bool empty() const { return queue_.empty(); }
    std::size_t pop() {
        const auto r=queue_.front(); queue_.pop_front(); queued_[r]=false; return r;
    }
    void notify(CellId v) {
        for(auto r:problem_.incident(v)) if(!queued_[r]) { queued_[r]=true; queue_.push_back(r); }
    }
private:
    const Problem& problem_;
    std::deque<std::size_t> queue_;
    std::vector<bool> queued_;
};
} // namespace mcr::detail

