#pragma once

#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <string>
#include <iomanip>

namespace aegis {

constexpr double PI = 3.14159265358979323846;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(double s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(double s) const { return Vec3(x / s, y / s, z / s); }

    double norm() const { return std::sqrt(x * x + y * y + z * z); }
    double norm_sq() const { return x * x + y * y + z * z; }
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

    static Vec3 from_spherical(double range, double azimuth_rad, double elevation_rad) {
        double r_cos_el = range * std::cos(elevation_rad);
        return Vec3(
            r_cos_el * std::sin(azimuth_rad),
            r_cos_el * std::cos(azimuth_rad),
            range * std::sin(elevation_rad)
        );
    }
};

enum class TrackStatus {
    TENTATIVE,
    CONFIRMED,
    COASTING,
    TERMINATED
};

enum class IffCategory {
    FRIEND,
    HOSTILE,
    UNKNOWN
};

inline const char* track_status_str(TrackStatus s) {
    switch (s) {
        case TrackStatus::TENTATIVE: return "TENTATIVE";
        case TrackStatus::CONFIRMED: return "CONFIRMED";
        case TrackStatus::COASTING: return "COASTING";
        case TrackStatus::TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

inline const char* iff_str(IffCategory c) {
    switch (c) {
        case IffCategory::FRIEND: return "FRIEND";
        case IffCategory::HOSTILE: return "HOSTILE";
        case IffCategory::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

struct RadarDetection {
    uint64_t id = 0;
    double timestamp = 0.0; // seconds
    double range = 0.0;     // meters
    double azimuth = 0.0;   // radians from North [0, 2*pi)
    double elevation = 0.0; // radians [-pi/2, pi/2]
    double doppler = 0.0;   // radial velocity m/s
    double snr = 0.0;       // signal-to-noise dB
    Vec3 cartesian;         // [x, y, z] computed

    void compute_cartesian() {
        cartesian = Vec3::from_spherical(range, azimuth, elevation);
    }
};

struct TrackState {
    uint32_t track_id = 0;
    TrackStatus status = TrackStatus::TENTATIVE;
    IffCategory iff = IffCategory::UNKNOWN;

    // State vector: [x, y, z, vx, vy, vz]
    std::array<double, 6> x{};
    // State error covariance matrix (6x6 symmetric)
    std::array<std::array<double, 6>, 6> P{};

    uint32_t hits = 1;
    uint32_t consecutive_misses = 0;
    uint32_t age_scans = 1;
    double last_timestamp = 0.0;
    double first_detected_time = 0.0;

    Vec3 position() const { return Vec3(x[0], x[1], x[2]); }
    Vec3 velocity() const { return Vec3(x[3], x[4], x[5]); }
    double speed() const { return velocity().norm(); }
};

} // namespace aegis
