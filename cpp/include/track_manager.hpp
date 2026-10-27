#pragma once

#include "radar_types.hpp"
#include "kalman_filter.hpp"
#include "data_association.hpp"
#include "ring_buffer.hpp"
#include <vector>
#include <memory>
#include <algorithm>

namespace aegis {

class MultiTargetTrackManager {
public:
    MultiTargetTrackManager(
        uint32_t confirmation_hits = 3,
        uint32_t max_consecutive_misses = 3,
        double process_noise_q = 4.0,
        double measurement_noise_r = 30.0
    ) : confirmation_hits_(confirmation_hits),
        max_consecutive_misses_(max_consecutive_misses),
        kf_(process_noise_q, measurement_noise_r),
        next_track_id_(1001) {}

    /**
     * @brief Two-Stage Priority Association Scan Processing:
     * Stage 1: Associate with Confirmed tracks
     * Stage 2: Associate with Tentative tracks
     * Stage 3: Initiate new Tentative tracks for unassigned detections
     */
    void process_scan(const std::vector<RadarDetection>& detections, double current_time) {
        // 1. Predict all active tracks to current scan timestamp
        for (auto& track : tracks_) {
            if (track.status != TrackStatus::TERMINATED) {
                kf_.predict(track, current_time);
            }
        }

        std::vector<bool> det_used(detections.size(), false);

        // Separate tracks into Confirmed and Tentative
        std::vector<size_t> confirmed_indices;
        std::vector<size_t> tentative_indices;
        for (size_t i = 0; i < tracks_.size(); ++i) {
            if (tracks_[i].status == TrackStatus::CONFIRMED || tracks_[i].status == TrackStatus::COASTING) {
                confirmed_indices.push_back(i);
            } else if (tracks_[i].status == TrackStatus::TENTATIVE) {
                tentative_indices.push_back(i);
            }
        }

        // --- STAGE 1: Associate Confirmed Tracks ---
        std::vector<TrackState> confirmed_tracks;
        for (size_t idx : confirmed_indices) confirmed_tracks.push_back(tracks_[idx]);

        auto assoc_conf = DataAssociator::associate(confirmed_tracks, detections, kf_);
        for (size_t k = 0; k < confirmed_indices.size(); ++k) {
            size_t orig_idx = confirmed_indices[k];
            int det_idx = assoc_conf.track_to_detection[k];
            if (det_idx >= 0 && static_cast<size_t>(det_idx) < detections.size()) {
                kf_.update(tracks_[orig_idx], detections[static_cast<size_t>(det_idx)]);
                tracks_[orig_idx].status = TrackStatus::CONFIRMED;
                det_used[static_cast<size_t>(det_idx)] = true;
            } else {
                tracks_[orig_idx].consecutive_misses++;
                tracks_[orig_idx].status = TrackStatus::COASTING;
                if (tracks_[orig_idx].consecutive_misses >= max_consecutive_misses_) {
                    tracks_[orig_idx].status = TrackStatus::TERMINATED;
                }
            }
        }

        // --- STAGE 2: Associate Tentative Tracks with Remaining Detections ---
        std::vector<RadarDetection> remaining_dets;
        std::vector<size_t> rem_to_orig_det;
        for (size_t j = 0; j < detections.size(); ++j) {
            if (!det_used[j]) {
                remaining_dets.push_back(detections[j]);
                rem_to_orig_det.push_back(j);
            }
        }

        std::vector<TrackState> tentative_tracks;
        for (size_t idx : tentative_indices) tentative_tracks.push_back(tracks_[idx]);

        auto assoc_tent = DataAssociator::associate(tentative_tracks, remaining_dets, kf_);
        for (size_t k = 0; k < tentative_indices.size(); ++k) {
            size_t orig_idx = tentative_indices[k];
            int rem_det_idx = assoc_tent.track_to_detection[k];
            if (rem_det_idx >= 0 && static_cast<size_t>(rem_det_idx) < remaining_dets.size()) {
                size_t actual_det_idx = rem_to_orig_det[static_cast<size_t>(rem_det_idx)];
                kf_.update(tracks_[orig_idx], detections[actual_det_idx]);
                det_used[actual_det_idx] = true;

                if (tracks_[orig_idx].hits >= confirmation_hits_) {
                    tracks_[orig_idx].status = TrackStatus::CONFIRMED;
                }
            } else {
                // Tentative track missed: reject immediately to suppress clutter
                tracks_[orig_idx].status = TrackStatus::TERMINATED;
            }
        }

        // --- STAGE 3: Initiate new Tentative tracks for unassigned detections ---
        for (size_t j = 0; j < detections.size(); ++j) {
            if (!det_used[j]) {
                init_tentative_track(detections[j]);
            }
        }

        // Prune terminated tracks
        prune_tracks();
    }

    const std::vector<TrackState>& get_tracks() const {
        return tracks_;
    }

    size_t active_track_count() const {
        return tracks_.size();
    }

    size_t confirmed_track_count() const {
        size_t c = 0;
        for (const auto& t : tracks_) {
            if (t.status == TrackStatus::CONFIRMED) ++c;
        }
        return c;
    }

    void clear() {
        tracks_.clear();
        next_track_id_ = 1001;
    }

private:
    void init_tentative_track(const RadarDetection& d) {
        TrackState new_track;
        new_track.track_id = next_track_id_++;
        new_track.status = TrackStatus::TENTATIVE;
        new_track.iff = classify_target(d);

        kf_.init_single(new_track, d);
        tracks_.push_back(new_track);
    }

    IffCategory classify_target(const RadarDetection& d) const {
        if (d.snr > 24.0) return IffCategory::FRIEND;
        if (d.snr > 14.0) return IffCategory::HOSTILE;
        return IffCategory::UNKNOWN;
    }

    void prune_tracks() {
        tracks_.erase(
            std::remove_if(tracks_.begin(), tracks_.end(), [](const TrackState& t) {
                return t.status == TrackStatus::TERMINATED;
            }),
            tracks_.end()
        );
    }

    uint32_t confirmation_hits_;
    uint32_t max_consecutive_misses_;
    KalmanFilter6D kf_;
    uint32_t next_track_id_;
    std::vector<TrackState> tracks_;
};

} // namespace aegis
