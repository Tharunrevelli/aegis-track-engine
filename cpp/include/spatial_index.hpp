#pragma once

#include "radar_types.hpp"
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

namespace aegis {

struct BoundingBox {
    double min_x;
    double min_y;
    double max_x;
    double max_y;

    bool contains(double x, double y) const {
        return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
    }

    bool intersects(const BoundingBox& o) const {
        return !(o.min_x > max_x || o.max_x < min_x || o.min_y > max_y || o.max_y < min_y);
    }
};

/**
 * @brief Spatial QuadTree for low-latency radar target partitioning and range queries
 */
class SpatialQuadTree {
public:
    static constexpr size_t NODE_CAPACITY = 16;
    static constexpr size_t MAX_DEPTH = 8;

    struct Entry {
        uint64_t id;
        double x;
        double y;
        size_t detection_index;
    };

    SpatialQuadTree(BoundingBox bounds, size_t depth = 0)
        : bounds_(bounds), depth_(depth), is_divided_(false) {
        entries_.reserve(NODE_CAPACITY);
    }

    bool insert(const Entry& entry) {
        if (!bounds_.contains(entry.x, entry.y)) {
            return false;
        }

        if (entries_.size() < NODE_CAPACITY || depth_ >= MAX_DEPTH) {
            entries_.push_back(entry);
            return true;
        }

        if (!is_divided_) {
            subdivide();
        }

        if (nw_->insert(entry)) return true;
        if (ne_->insert(entry)) return true;
        if (sw_->insert(entry)) return true;
        if (se_->insert(entry)) return true;

        return false;
    }

    void query_range(const BoundingBox& range, std::vector<Entry>& results) const {
        if (!bounds_.intersects(range)) {
            return;
        }

        for (const auto& entry : entries_) {
            if (range.contains(entry.x, entry.y)) {
                results.push_back(entry);
            }
        }

        if (is_divided_) {
            nw_->query_range(range, results);
            ne_->query_range(range, results);
            sw_->query_range(range, results);
            se_->query_range(range, results);
        }
    }

    void query_radius(double cx, double cy, double radius, std::vector<Entry>& results) const {
        BoundingBox box{cx - radius, cy - radius, cx + radius, cy + radius};
        std::vector<Entry> candidates;
        query_range(box, candidates);

        double r_sq = radius * radius;
        for (const auto& cand : candidates) {
            double dx = cand.x - cx;
            double dy = cand.y - cy;
            if (dx * dx + dy * dy <= r_sq) {
                results.push_back(cand);
            }
        }
    }

    void clear() {
        entries_.clear();
        is_divided_ = false;
        nw_.reset();
        ne_.reset();
        sw_.reset();
        se_.reset();
    }

private:
    void subdivide() {
        double mid_x = (bounds_.min_x + bounds_.max_x) * 0.5;
        double mid_y = (bounds_.min_y + bounds_.max_y) * 0.5;

        nw_ = std::make_unique<SpatialQuadTree>(BoundingBox{bounds_.min_x, mid_y, mid_x, bounds_.max_y}, depth_ + 1);
        ne_ = std::make_unique<SpatialQuadTree>(BoundingBox{mid_x, mid_y, bounds_.max_x, bounds_.max_y}, depth_ + 1);
        sw_ = std::make_unique<SpatialQuadTree>(BoundingBox{bounds_.min_x, bounds_.min_y, mid_x, mid_y}, depth_ + 1);
        se_ = std::make_unique<SpatialQuadTree>(BoundingBox{mid_x, bounds_.min_y, bounds_.max_x, mid_y}, depth_ + 1);

        is_divided_ = true;

        // Redistribute existing entries
        auto old_entries = std::move(entries_);
        entries_.clear();
        for (const auto& e : old_entries) {
            if (nw_->insert(e)) continue;
            if (ne_->insert(e)) continue;
            if (sw_->insert(e)) continue;
            if (se_->insert(e)) continue;
            entries_.push_back(e);
        }
    }

    BoundingBox bounds_;
    size_t depth_;
    bool is_divided_;
    std::vector<Entry> entries_;
    std::unique_ptr<SpatialQuadTree> nw_;
    std::unique_ptr<SpatialQuadTree> ne_;
    std::unique_ptr<SpatialQuadTree> sw_;
    std::unique_ptr<SpatialQuadTree> se_;
};

} // namespace aegis
