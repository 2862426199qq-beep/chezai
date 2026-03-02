#ifndef RADAR_PROCESSOR_H
#define RADAR_PROCESSOR_H

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <complex>

namespace RadarConfig {
    constexpr double FC_HZ       = 60.0e6;
    constexpr double BW_HZ       = 60.0e6;
    constexpr double C_MPS       = 3.0e8;
    constexpr int    N_SAMPLES   = 64;
    constexpr double FRAME_MS    = 50.0;
    constexpr double RANGE_RES_M = C_MPS / (2.0 * BW_HZ);
    constexpr double RANGE_MAX_M = (N_SAMPLES / 2) * RANGE_RES_M;
    constexpr int    FRAME_SIZE  = 264;  /* 保持 264 */
}

struct RadarTarget {
    int    bin;
    double range_m;
    double velocity_mps;
    double amplitude;
    double phase_rad;
    double angle_deg;

    std::string label() const {
        if (range_m >= 8.0  && range_m <= 12.0) return "行人";
        if (range_m >= 18.0 && range_m <= 22.0) return "汽车";
        if (range_m >= 48.0 && range_m <= 52.0) return "路障";
        return "未知";
    }
};

#pragma pack(push, 1)
struct IQPoint {
    int16_t i;
    int16_t q;
};
#pragma pack(pop)

class RadarProcessor {
public:
    struct CfarParams {
        int    guard_cells;
        int    train_cells;
        double threshold_dB;
        CfarParams() : guard_cells(2), train_cells(4), threshold_dB(15.0) {}
    };

    RadarProcessor(CfarParams params = CfarParams());

    std::vector<RadarTarget> process(const IQPoint raw_iq[RadarConfig::N_SAMPLES],
                                     uint16_t frame_seq,
                                     double scan_angle_deg);

    const std::vector<double>& get_magnitude_spectrum() const { return magnitude_; }
    int frame_count() const { return frame_count_; }
    bool is_new_frame() const { return is_new_frame_; }

private:
    using Complex = std::complex<double>;

    void fft(std::vector<Complex>& x);
    void bit_reverse(std::vector<Complex>& x);
    std::vector<RadarTarget> cfar_detect(const std::vector<double>& mag,
                                          const std::vector<double>& phase,
                                          double angle_deg);
    double calc_velocity(double phase_now, double phase_prev, int seq_gap);

    CfarParams            cfar_params_;
    std::vector<double>   magnitude_;
    std::vector<double>   prev_phase_;
    bool                  has_prev_phase_;
    int                   frame_count_;
    uint16_t              prev_seq_;
    bool                  has_prev_seq_;
    bool                  is_new_frame_;
    std::vector<RadarTarget> latest_targets_;
};

#endif