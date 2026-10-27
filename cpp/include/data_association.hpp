#pragma once

#include "radar_types.hpp"
#include "kalman_filter.hpp"
#include <vector>
#include <queue>
#include <cmath>
#include <algorithm>

namespace aegis {

constexpr double TENTATIVE_GATE = 45.0; // Chi-squared gate for track initiation (~3 DOF)
constexpr double CONFIRMED_GATE = 25.0; // Chi-squared gate for confirmed tracks

struct CandidateEdge {
    size_t track_idx;
    size_t det_idx;
    double distance_sq;

    bool operator>(const CandidateEdge& o) const {
        return distance_sq > o.distance_sq; // Min-heap order
    }
};

struct AssociationResult {
    std::vector<int> track_to_detection;
    std::vector<size_t> unassigned_detections;
    std::vector<size_t> unassigned_tracks;
};

/**
 * @brief Global Nearest Neighbor (GNN) Statistical Data Associator
 * Employs Priority-Queue driven Minimum-Distance Bipartite Matching with Chi-squared gating.
 */
class DataAssociator {
public:
    static AssociationResult associate(
        const std::vector<TrackState>& tracks,
        const std::vector<RadarDetection>& detections,
        const KalmanFilter6D& kf
    ) {
        AssociationResult result;
        if (tracks.empty()) {
            for (size_t j = 0; j < detections.size(); ++j) {
                result.unassigned_detections.push_back(j);
            }
            return result;
        }

        if (detections.empty()) {
            for (size_t i = 0; i < tracks.size(); ++i) {
                result.unassigned_tracks.push_back(i);
                result.track_to_detection.push_back(-1);
            }
            return result;
        }

        size_t num_tracks = tracks.size();
        size_t num_dets = detections.size();

        result.track_to_detection.assign(num_tracks, -1);
        std::priority_queue<CandidateEdge, std::vector<CandidateEdge>, std::greater<CandidateEdge>> min_heap;

        // Gate evaluation and edge population
        for (size_t i = 0; i < num_tracks; ++i) {
            double gate = (tracks[i].status == TrackStatus::CONFIRMED) ? CONFIRMED_GATE : TENTATIVE_GATE;
            for (size_t j = 0; j < num_dets; ++j) {
                double d_sq = kf.mahalanobis_distance_sq(tracks[i], detections[j]);
                if (d_sq <= gate) {
                    min_heap.push({i, j, d_sq});
                }
            }
        }

        // Optimal greedy competitive assignment
        std::vector<bool> track_assigned(num_tracks, false);
        std::vector<bool> det_assigned(num_dets, false);

        while (!min_heap.empty()) {
            auto edge = min_heap.top();
            min_heap.pop();

            if (!track_assigned[edge.track_idx] && !det_assigned[edge.det_idx]) {
                track_assigned[edge.track_idx] = true;
                det_assigned[edge.det_idx] = true;
                result.track_to_detection[edge.track_idx] = static_cast<int>(edge.det_idx);
            }
        }

        // Categorize unassigned tracks and detections
        for (size_t i = 0; i < num_tracks; ++i) {
            if (!track_assigned[i]) {
                result.unassigned_tracks.push_back(i);
            }
        }

        for (size_t j = 0; j < num_dets; ++j) {
            if (!det_assigned[j]) {
                result.unassigned_detections.push_back(j);
            }
        }

        return result;
    }
};

} // namespace aegis
