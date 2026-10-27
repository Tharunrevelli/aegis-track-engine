#include "radar_types.hpp"
#include "ring_buffer.hpp"
#include "spatial_index.hpp"
#include "kalman_filter.hpp"
#include "data_association.hpp"
#include "track_manager.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

using namespace aegis;

void test_ring_buffer() {
    CircularRingBuffer<int, 4> rb;
    assert(rb.empty());
    assert(rb.capacity() == 4);

    assert(rb.push(10));
    assert(rb.push(20));
    assert(rb.push(30));
    assert(rb.push(40));
    assert(rb.full());
    assert(!rb.push(50)); // Overflow rejected

    int val = 0;
    assert(rb.pop(val) && val == 10);
    assert(rb.pop(val) && val == 20);
    assert(rb.push(60));
    assert(rb.pop(val) && val == 30);
    assert(rb.pop(val) && val == 40);
    assert(rb.pop(val) && val == 60);
    assert(rb.empty());
    std::cout << "[PASS] CircularRingBuffer tests passed.\n";
}

void test_spatial_quadtree() {
    BoundingBox bb{-1000.0, -1000.0, 1000.0, 1000.0};
    SpatialQuadTree qt(bb);

    qt.insert({1, 100.0, 100.0, 0});
    qt.insert({2, -500.0, -500.0, 1});
    qt.insert({3, 105.0, 95.0, 2});
    qt.insert({4, 800.0, 800.0, 3});

    std::vector<SpatialQuadTree::Entry> results;
    qt.query_radius(100.0, 100.0, 20.0, results);
    assert(results.size() == 2); // Entries 1 and 3
    std::cout << "[PASS] SpatialQuadTree tests passed.\n";
}

void test_kalman_filter() {
    KalmanFilter6D kf(1.0, 10.0);
    TrackState trk;

    RadarDetection d1;
    d1.timestamp = 0.0;
    d1.cartesian = Vec3(0.0, 0.0, 1000.0);

    RadarDetection d2;
    d2.timestamp = 1.0;
    d2.cartesian = Vec3(100.0, 0.0, 1000.0);

    kf.init(trk, d1, d2);
    assert(std::abs(trk.x[0] - 100.0) < 1e-4);
    assert(std::abs(trk.x[3] - 100.0) < 1e-4); // 100 m/s velocity in x

    // Predict 1 second ahead
    kf.predict(trk, 2.0);
    assert(std::abs(trk.x[0] - 200.0) < 1e-4);

    // Update with measurement at x = 205
    RadarDetection d3;
    d3.timestamp = 2.0;
    d3.cartesian = Vec3(205.0, 0.0, 1000.0);
    kf.update(trk, d3);

    // State should adjust towards 205
    assert(trk.x[0] > 200.0 && trk.x[0] < 205.0);
    std::cout << "[PASS] KalmanFilter6D tests passed.\n";
}

void test_data_association() {
    KalmanFilter6D kf(4.0, 25.0);
    TrackState trk;
    trk.track_id = 1;
    trk.status = TrackStatus::CONFIRMED;
    trk.x = {1000.0, 1000.0, 500.0, 50.0, 0.0, 0.0};
    trk.last_timestamp = 1.0;
    for (int i = 0; i < 6; ++i) trk.P[i][i] = 30.0;

    RadarDetection d1;
    d1.id = 1;
    d1.timestamp = 2.0;
    d1.cartesian = Vec3(1052.0, 1001.0, 501.0); // Very close to predicted (1050, 1000, 500)

    RadarDetection d2;
    d2.id = 2;
    d2.timestamp = 2.0;
    d2.cartesian = Vec3(9000.0, 9000.0, 2000.0); // Far clutter

    kf.predict(trk, 2.0); // Predict track position to measurement epoch
    auto result = DataAssociator::associate({trk}, {d1, d2}, kf);
    assert(result.track_to_detection.size() == 1);
    assert(result.track_to_detection[0] == 0); // Assigned to d1
    assert(result.unassigned_detections.size() == 1);
    assert(result.unassigned_detections[0] == 1); // d2 unassigned

    std::cout << "[PASS] DataAssociator priority-queue matching passed.\n";
}

void test_track_manager_lifecycle() {
    MultiTargetTrackManager tm(2, 2);

    RadarDetection d1;
    d1.id = 1;
    d1.timestamp = 1.0;
    d1.range = 1500.0;
    d1.azimuth = 0.785;
    d1.elevation = 0.34;
    d1.doppler = 50.0;
    d1.snr = 25.0;
    d1.compute_cartesian();

    tm.process_scan({d1}, 1.0);
    assert(tm.active_track_count() == 1);
    assert(tm.get_tracks()[0].status == TrackStatus::TENTATIVE);

    RadarDetection d2 = d1;
    d2.id = 2;
    d2.timestamp = 2.0;
    d2.cartesian = d1.cartesian + Vec3(50.0, 0.0, 0.0);
    tm.process_scan({d2}, 2.0);

    assert(tm.confirmed_track_count() == 1);
    assert(tm.get_tracks()[0].status == TrackStatus::CONFIRMED);

    // Missing detections -> Coasting then Terminated
    tm.process_scan({}, 3.0);
    assert(tm.get_tracks()[0].status == TrackStatus::COASTING);

    tm.process_scan({}, 4.0);
    assert(tm.active_track_count() == 0);

    std::cout << "[PASS] MultiTargetTrackManager lifecycle tests passed.\n";
}

int main() {
    std::cout << "--- Running Aegis-Track Unit Test Suite ---\n";
    test_ring_buffer();
    test_spatial_quadtree();
    test_kalman_filter();
    test_data_association();
    test_track_manager_lifecycle();
    std::cout << "--- ALL AEGIS-TRACK UNIT TESTS PASSED ---\n";
    return 0;
}
