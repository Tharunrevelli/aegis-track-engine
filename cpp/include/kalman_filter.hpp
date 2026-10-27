#pragma once

#include "radar_types.hpp"
#include <array>
#include <cmath>

namespace aegis {

/**
 * @brief 6-DOF Kinematic Kalman Filter for 3D Radar Target Tracking
 * Implements Constant Velocity (CV) model with continuous process noise.
 */
class KalmanFilter6D {
public:
    KalmanFilter6D(double process_noise_q = 4.0, double measurement_noise_r = 25.0)
        : q_(process_noise_q), r_(measurement_noise_r) {}

    /**
     * @brief Initialize track state from first two detections
     */
    void init(TrackState& track, const RadarDetection& d1, const RadarDetection& d2) {
        double dt = d2.timestamp - d1.timestamp;
        if (dt <= 1e-4) dt = 1.0;

        track.x[0] = d2.cartesian.x;
        track.x[1] = d2.cartesian.y;
        track.x[2] = d2.cartesian.z;
        track.x[3] = (d2.cartesian.x - d1.cartesian.x) / dt;
        track.x[4] = (d2.cartesian.y - d1.cartesian.y) / dt;
        track.x[5] = (d2.cartesian.z - d1.cartesian.z) / dt;

        // Initial error covariance P0
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                track.P[i][j] = 0.0;
            }
        }
        track.P[0][0] = r_;
        track.P[1][1] = r_;
        track.P[2][2] = r_;
        track.P[3][3] = 2.0 * r_ / (dt * dt);
        track.P[4][4] = 2.0 * r_ / (dt * dt);
        track.P[5][5] = 2.0 * r_ / (dt * dt);

        track.last_timestamp = d2.timestamp;
        track.first_detected_time = d1.timestamp;
        track.hits = 2;
        track.consecutive_misses = 0;
        track.age_scans = 2;
    }

    /**
     * @brief Single-detection initialization (with default prior velocity)
     */
    void init_single(TrackState& track, const RadarDetection& d) {
        track.x[0] = d.cartesian.x;
        track.x[1] = d.cartesian.y;
        track.x[2] = d.cartesian.z;
        // Doppler can estimate radial velocity component
        double vr = d.doppler;
        double r_cos_el = std::cos(d.elevation);
        track.x[3] = vr * std::sin(d.azimuth) * r_cos_el;
        track.x[4] = vr * std::cos(d.azimuth) * r_cos_el;
        track.x[5] = vr * std::sin(d.elevation);

        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                track.P[i][j] = 0.0;
            }
        }
        track.P[0][0] = r_;
        track.P[1][1] = r_;
        track.P[2][2] = r_;
        track.P[3][3] = 40000.0; // 200 m/s initial velocity uncertainty
        track.P[4][4] = 40000.0;
        track.P[5][5] = 10000.0;

        track.last_timestamp = d.timestamp;
        track.first_detected_time = d.timestamp;
        track.hits = 1;
        track.consecutive_misses = 0;
        track.age_scans = 1;
    }

    /**
     * @brief Predict state and covariance to target timestamp: x_{k|k-1} = F x, P_{k|k-1} = F P F^T + Q
     */
    void predict(TrackState& track, double current_time) {
        double dt = current_time - track.last_timestamp;
        if (dt < 0.0) return;

        // State prediction x_pred = F * x
        track.x[0] += track.x[3] * dt;
        track.x[1] += track.x[4] * dt;
        track.x[2] += track.x[5] * dt;

        // Covariance prediction P = F * P * F^T + Q
        // F is Identity with dt at (0,3), (1,4), (2,5)
        std::array<std::array<double, 6>, 6> P_temp = track.P;

        // F * P
        for (int j = 0; j < 6; ++j) {
            P_temp[0][j] += dt * track.P[3][j];
            P_temp[1][j] += dt * track.P[4][j];
            P_temp[2][j] += dt * track.P[5][j];
        }

        // (F * P) * F^T
        std::array<std::array<double, 6>, 6> P_pred = P_temp;
        for (int i = 0; i < 6; ++i) {
            P_pred[i][0] += dt * P_temp[i][3];
            P_pred[i][1] += dt * P_temp[i][4];
            P_pred[i][2] += dt * P_temp[i][5];
        }

        // Discrete Process Noise Q(dt)
        double dt2 = dt * dt;
        double dt3 = dt2 * dt / 2.0;
        double dt4 = dt2 * dt2 / 4.0;

        for (int k = 0; k < 3; ++k) {
            P_pred[k][k]     += q_ * dt4;
            P_pred[k][k + 3] += q_ * dt3;
            P_pred[k + 3][k] += q_ * dt3;
            P_pred[k + 3][k + 3] += q_ * dt2;
        }

        track.P = P_pred;
        track.last_timestamp = current_time;
    }

    /**
     * @brief Compute Mahalanobis distance squared d_M^2 for gating check
     */
    double mahalanobis_distance_sq(const TrackState& track, const RadarDetection& d) const {
        // Innovation y = z - H * x (position error)
        double y[3] = {
            d.cartesian.x - track.x[0],
            d.cartesian.y - track.x[1],
            d.cartesian.z - track.x[2]
        };

        // Innovation covariance S = H * P * H^T + R
        // S is 3x3 submatrix of P plus diagonal R
        double S[3][3];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                S[i][j] = track.P[i][j];
            }
            S[i][i] += r_;
        }

        // Invert 3x3 matrix S
        double S_inv[3][3];
        if (!invert3x3(S, S_inv)) {
            return 1e9; // degenerate covariance
        }

        // d_M^2 = y^T * S_inv * y
        double Sy[3] = {
            S_inv[0][0] * y[0] + S_inv[0][1] * y[1] + S_inv[0][2] * y[2],
            S_inv[1][0] * y[0] + S_inv[1][1] * y[1] + S_inv[1][2] * y[2],
            S_inv[2][0] * y[0] + S_inv[2][1] * y[1] + S_inv[2][2] * y[2]
        };

        return y[0] * Sy[0] + y[1] * Sy[1] + y[2] * Sy[2];
    }

    /**
     * @brief Update track state with associated radar measurement: x_{k|k} = x + K y, P_{k|k} = (I - KH) P
     */
    void update(TrackState& track, const RadarDetection& d) {
        double dt = d.timestamp - track.last_timestamp;
        if (track.hits == 1 && dt > 1e-4) {
            // First association: initialize velocity from kinematic baseline
            track.x[3] = (d.cartesian.x - track.x[0]) / dt;
            track.x[4] = (d.cartesian.y - track.x[1]) / dt;
            track.x[5] = (d.cartesian.z - track.x[2]) / dt;
            track.x[0] = d.cartesian.x;
            track.x[1] = d.cartesian.y;
            track.x[2] = d.cartesian.z;

            track.P[0][0] = r_;
            track.P[1][1] = r_;
            track.P[2][2] = r_;
            track.P[3][3] = 2.0 * r_ / (dt * dt);
            track.P[4][4] = 2.0 * r_ / (dt * dt);
            track.P[5][5] = 2.0 * r_ / (dt * dt);

            track.hits = 2;
            track.consecutive_misses = 0;
            track.age_scans = 2;
            track.last_timestamp = d.timestamp;
            return;
        }

        double y[3] = {
            d.cartesian.x - track.x[0],
            d.cartesian.y - track.x[1],
            d.cartesian.z - track.x[2]
        };

        // S = H * P * H^T + R
        double S[3][3];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                S[i][j] = track.P[i][j];
            }
            S[i][i] += r_;
        }

        double S_inv[3][3];
        if (!invert3x3(S, S_inv)) {
            return;
        }

        // Kalman gain K = P * H^T * S_inv (6x3 matrix)
        // Since H = [I_3 | 0_3], P * H^T is first 3 columns of P
        double K[6][3];
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 3; ++j) {
                K[i][j] = track.P[i][0] * S_inv[0][j] +
                          track.P[i][1] * S_inv[1][j] +
                          track.P[i][2] * S_inv[2][j];
            }
        }

        // State update: x = x + K * y
        for (int i = 0; i < 6; ++i) {
            track.x[i] += K[i][0] * y[0] + K[i][1] * y[1] + K[i][2] * y[2];
        }

        // Covariance update: P = (I - K * H) * P
        // KH is 6x6, where (KH)[i][j] = K[i][j] for j < 3, 0 otherwise
        std::array<std::array<double, 6>, 6> P_new = track.P;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                double kh_p = K[i][0] * track.P[0][j] +
                              K[i][1] * track.P[1][j] +
                              K[i][2] * track.P[2][j];
                P_new[i][j] -= kh_p;
            }
        }

        track.P = P_new;
        track.hits++;
        track.consecutive_misses = 0;
        track.age_scans++;
        track.last_timestamp = d.timestamp;
    }

private:
    static bool invert3x3(const double A[3][3], double Inv[3][3]) {
        double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
                     A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                     A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);

        if (std::abs(det) < 1e-12) return false;
        double invdet = 1.0 / det;

        Inv[0][0] =  (A[1][1] * A[2][2] - A[1][2] * A[2][1]) * invdet;
        Inv[0][1] = -(A[0][1] * A[2][2] - A[0][2] * A[2][1]) * invdet;
        Inv[0][2] =  (A[0][1] * A[1][2] - A[0][2] * A[1][1]) * invdet;

        Inv[1][0] = -(A[1][0] * A[2][2] - A[1][2] * A[2][0]) * invdet;
        Inv[1][1] =  (A[0][0] * A[2][2] - A[0][2] * A[2][0]) * invdet;
        Inv[1][2] = -(A[0][0] * A[1][2] - A[0][2] * A[1][0]) * invdet;

        Inv[2][0] =  (A[1][0] * A[2][1] - A[1][1] * A[2][0]) * invdet;
        Inv[2][1] = -(A[0][0] * A[2][1] - A[0][1] * A[2][0]) * invdet;
        Inv[2][2] =  (A[0][0] * A[1][1] - A[0][1] * A[1][0]) * invdet;

        return true;
    }

    double q_;
    double r_;
};

} // namespace aegis
