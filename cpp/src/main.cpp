#include "radar_types.hpp"
#include "ring_buffer.hpp"
#include "spatial_index.hpp"
#include "kalman_filter.hpp"
#include "data_association.hpp"
#include "track_manager.hpp"

#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>

using namespace aegis;

struct SimulatedAircraft {
    uint32_t id;
    std::string callsign;
    Vec3 pos;
    Vec3 vel;
    IffCategory iff;
    bool maneuvers;

    void update(double dt, double current_time) {
        if (maneuvers && current_time > 15.0 && current_time < 35.0) {
            // Perform 4-G lateral banking turn
            double turn_rate = 0.08; // rad/s
            double vx_new = vel.x * std::cos(turn_rate * dt) - vel.y * std::sin(turn_rate * dt);
            double vy_new = vel.x * std::sin(turn_rate * dt) + vel.y * std::cos(turn_rate * dt);
            vel.x = vx_new;
            vel.y = vy_new;
        }
        pos = pos + vel * dt;
    }
};

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  AEGIS-TRACK: REAL-TIME RADAR TARGET TRACKING ENGINE     \n";
    std::cout << "  High-Performance Sensor Fusion & Track Lifecycle System \n";
    std::cout << "==========================================================\n\n";

    // Initialize simulated targets
    std::vector<SimulatedAircraft> aircraft = {
        {101, "CIVIL-AIR-42",  Vec3(-35000.0,  20000.0, 9000.0), Vec3(180.0, -90.0, 0.0),  IffCategory::FRIEND,  false},
        {102, "MIG-29-INTERC", Vec3( 25000.0,  30000.0, 6000.0), Vec3(-320.0, -180.0, 10.0), IffCategory::HOSTILE, true},
        {103, "CRUISE-MISSILE",Vec3(-15000.0, -28000.0,  180.0), Vec3(220.0, 160.0, 0.0),   IffCategory::HOSTILE, false},
        {104, "TACTICAL-UAV",  Vec3( 10000.0, -12000.0, 3500.0), Vec3(-35.0, 40.0, 0.0),    IffCategory::UNKNOWN, false}
    };

    // Radar measurement noise: 15m range std-dev, 3 mrad angle std-dev -> cross-range ~60-120m at medium range
    // Set measurement noise variance R = 1600.0 (~40m effective std-dev)
    MultiTargetTrackManager track_mgr(3, 3, 5.0, 1600.0);
    CircularRingBuffer<RadarDetection, 4096> ingestion_buffer;

    std::mt19937 rng(42);
    std::normal_distribution<double> range_noise(0.0, 15.0);   // 15m range std-dev
    std::normal_distribution<double> angle_noise(0.0, 0.003);  // 3 milliradians angle std-dev
    std::uniform_real_distribution<double> clutter_dist(-40000.0, 40000.0);
    std::uniform_real_distribution<double> clutter_alt(50.0, 8000.0);

    const double scan_period = 1.0; // 1-second radar rotation period
    const int total_scans = 60;
    uint64_t detection_id_seq = 1;

    std::ofstream json_out("radar_telemetry_output.json");
    json_out << "{\n  \"scans\": [\n";

    double total_processing_time_us = 0.0;
    size_t total_detections_processed = 0;
    double cumulative_position_error = 0.0;
    size_t evaluated_points = 0;

    std::cout << "[SIMULATION] Commencing 60-scan tactical pulse-doppler radar sweeps...\n";

    for (int scan = 0; scan < total_scans; ++scan) {
        double current_time = scan * scan_period;

        // Update ground truth targets
        for (auto& ac : aircraft) {
            ac.update(scan_period, current_time);
        }

        // Generate radar detections with sensor noise
        std::vector<RadarDetection> scan_detections;

        for (const auto& ac : aircraft) {
            double true_range = ac.pos.norm();
            if (true_range > 60000.0) continue; // Beyond max radar instrumented range

            double true_azimuth = std::atan2(ac.pos.x, ac.pos.y);
            if (true_azimuth < 0.0) true_azimuth += 2.0 * PI;
            double true_elevation = std::asin(ac.pos.z / true_range);

            RadarDetection det;
            det.id = detection_id_seq++;
            det.timestamp = current_time;
            det.range = true_range + range_noise(rng);
            det.azimuth = true_azimuth + angle_noise(rng);
            det.elevation = true_elevation + angle_noise(rng);
            det.doppler = (ac.pos.dot(ac.vel)) / true_range;
            det.snr = (ac.iff == IffCategory::HOSTILE) ? 18.5 : 28.0;
            det.compute_cartesian();

            scan_detections.push_back(det);
        }

        // Inject 4-6 random radar clutter false alarms per scan
        int clutter_count = 4 + (scan % 3);
        for (int c = 0; c < clutter_count; ++c) {
            RadarDetection clutter;
            clutter.id = detection_id_seq++;
            clutter.timestamp = current_time;
            Vec3 c_pos(clutter_dist(rng), clutter_dist(rng), clutter_alt(rng));
            clutter.range = c_pos.norm();
            clutter.azimuth = std::atan2(c_pos.x, c_pos.y);
            if (clutter.azimuth < 0.0) clutter.azimuth += 2.0 * PI;
            clutter.elevation = std::asin(c_pos.z / clutter.range);
            clutter.doppler = 0.0;
            clutter.snr = 11.0; // low SNR typical of ground/sea clutter
            clutter.compute_cartesian();
            scan_detections.push_back(clutter);
        }

        // Enqueue into high-speed ring buffer
        for (const auto& det : scan_detections) {
            ingestion_buffer.push(det);
        }

        // Dequeue from ring buffer to process batch
        std::vector<RadarDetection> batch_to_process;
        RadarDetection d;
        while (ingestion_buffer.pop(d)) {
            batch_to_process.push_back(d);
        }

        total_detections_processed += batch_to_process.size();

        // Benchmark track processing latency
        auto t_start = std::chrono::high_resolution_clock::now();
        track_mgr.process_scan(batch_to_process, current_time);
        auto t_end = std::chrono::high_resolution_clock::now();

        double elapsed_us = std::chrono::duration<double, std::nano>(t_end - t_start).count() / 1000.0;
        total_processing_time_us += (elapsed_us > 0.05 ? elapsed_us : 0.5);

        // Calculate tracking error against ground truth
        const auto& tracks = track_mgr.get_tracks();
        for (const auto& ac : aircraft) {
            for (const auto& trk : tracks) {
                if (trk.status == TrackStatus::CONFIRMED) {
                    double dist = (trk.position() - ac.pos).norm();
                    if (dist < 300.0) { // Associated with ground truth
                        cumulative_position_error += dist * dist;
                        evaluated_points++;
                        break;
                    }
                }
            }
        }

        // Export scan details to JSON
        json_out << "    {\n";
        json_out << "      \"scan_index\": " << scan << ",\n";
        json_out << "      \"time_sec\": " << std::fixed << std::setprecision(1) << current_time << ",\n";
        json_out << "      \"latency_us\": " << std::fixed << std::setprecision(2) << elapsed_us << ",\n";
        json_out << "      \"detection_count\": " << batch_to_process.size() << ",\n";
        json_out << "      \"confirmed_tracks\": " << track_mgr.confirmed_track_count() << ",\n";
        json_out << "      \"tracks\": [\n";

        for (size_t t = 0; t < tracks.size(); ++t) {
            const auto& trk = tracks[t];
            json_out << "        {\n";
            json_out << "          \"track_id\": " << trk.track_id << ",\n";
            json_out << "          \"status\": \"" << track_status_str(trk.status) << "\",\n";
            json_out << "          \"iff\": \"" << iff_str(trk.iff) << "\",\n";
            json_out << "          \"x\": " << std::fixed << std::setprecision(1) << trk.x[0] << ",\n";
            json_out << "          \"y\": " << trk.x[1] << ",\n";
            json_out << "          \"z\": " << trk.x[2] << ",\n";
            json_out << "          \"vx\": " << std::setprecision(2) << trk.x[3] << ",\n";
            json_out << "          \"vy\": " << trk.x[4] << ",\n";
            json_out << "          \"vz\": " << trk.x[5] << ",\n";
            json_out << "          \"speed\": " << trk.speed() << ",\n";
            json_out << "          \"hits\": " << trk.hits << ",\n";
            json_out << "          \"misses\": " << trk.consecutive_misses << "\n";
            json_out << "        }" << (t + 1 < tracks.size() ? "," : "") << "\n";
        }
        json_out << "      ]\n";
        json_out << "    }" << (scan + 1 < total_scans ? "," : "") << "\n";

        if (scan % 15 == 0 || scan == total_scans - 1) {
            std::cout << "  Scan " << std::setw(2) << scan 
                      << " | Detections: " << std::setw(2) << batch_to_process.size()
                      << " | Confirmed Tracks: " << track_mgr.confirmed_track_count()
                      << " | Latency: " << std::fixed << std::setprecision(1) << elapsed_us << " us\n";
        }
    }

    json_out << "  ]\n}\n";
    json_out.close();

    double avg_latency_us = total_processing_time_us / total_scans;
    double throughput = (total_detections_processed / total_processing_time_us) * 1e6;
    double rmse = evaluated_points > 0 ? std::sqrt(cumulative_position_error / evaluated_points) : 0.0;

    std::cout << "\n------------------ BENCHMARK RESULTS ------------------\n";
    std::cout << "  Total Scans Processed       : " << total_scans << "\n";
    std::cout << "  Total Detections Ingested   : " << total_detections_processed << "\n";
    std::cout << "  Average Scan Latency        : " << std::fixed << std::setprecision(2) << avg_latency_us << " microseconds\n";
    std::cout << "  Peak Ingestion Throughput   : " << std::fixed << std::setprecision(0) << throughput << " detections/sec\n";
    std::cout << "  Tracking Kinematic RMSE     : " << std::fixed << std::setprecision(2) << rmse << " meters\n";
    std::cout << "  Telemetry export saved to   : radar_telemetry_output.json\n";
    std::cout << "--------------------------------------------------------\n";

    return 0;
}
