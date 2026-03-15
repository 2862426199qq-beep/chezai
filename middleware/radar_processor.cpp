#include "radar_processor.h"
#include <algorithm>

using namespace RadarConfig;

RadarProcessor::RadarProcessor(CfarParams params)
    : cfar_params_(params)
    , magnitude_(N_SAMPLES, 0.0)
    , prev_phase_(N_SAMPLES, 0.0)
    , has_prev_phase_(false)
    , frame_count_(0)
    , prev_seq_(0)
    , has_prev_seq_(false)
    , is_new_frame_(false)
{
}

std::vector<RadarTarget> RadarProcessor::process(
    const IQPoint raw_iq[N_SAMPLES],
    uint16_t frame_seq,
    double scan_angle_deg)
{
    frame_count_++;

    if (has_prev_seq_ && frame_seq == prev_seq_) {
        is_new_frame_ = false;
        return latest_targets_;
    }

    is_new_frame_ = true;

    int seq_gap = 1;
    if (has_prev_seq_) {
        seq_gap = (int)(uint16_t)(frame_seq - prev_seq_);
        if (seq_gap <= 0 || seq_gap > 100) seq_gap = 1;
    }

    std::vector<Complex> fft_data(N_SAMPLES);
    for (int n = 0; n < N_SAMPLES; n++) {
        /* Hann 窗：减小旁瓣，避免强近场信号泄漏到远距离 bin */
        double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * n / (N_SAMPLES - 1)));
        fft_data[n] = Complex(static_cast<double>(raw_iq[n].i) * w,
                              static_cast<double>(raw_iq[n].q) * w);
    }

    fft(fft_data);

    std::vector<double> cur_phase(N_SAMPLES);
    for (int n = 0; n < N_SAMPLES; n++) {
        magnitude_[n] = std::abs(fft_data[n]);
        cur_phase[n]  = std::arg(fft_data[n]);
    }

    std::vector<RadarTarget> targets = cfar_detect(magnitude_, cur_phase, scan_angle_deg);

    if (has_prev_phase_) {
        for (size_t i = 0; i < targets.size(); i++) {
            int b = targets[i].bin;
            targets[i].velocity_mps = calc_velocity(
                cur_phase[b], prev_phase_[b], seq_gap);
        }
    }

    prev_phase_ = cur_phase;
    has_prev_phase_ = true;
    prev_seq_ = frame_seq;
    has_prev_seq_ = true;
    latest_targets_ = targets;

    return latest_targets_;
}

/* FFT */
void RadarProcessor::bit_reverse(std::vector<Complex>& x)
{
    int n = static_cast<int>(x.size());
    for (int i = 1, j = 0; i < n; i++) {
        for (int k = n >> 1; k > (j ^= k); k >>= 1) ;
        if (i < j) std::swap(x[i], x[j]);
    }
}

void RadarProcessor::fft(std::vector<Complex>& x)
{
    int n = static_cast<int>(x.size());
    bit_reverse(x);
    for (int step = 2; step <= n; step <<= 1) {
        int half = step >> 1;
        double angle_step = -2.0 * M_PI / step;
        for (int group = 0; group < n; group += step) {
            for (int pair = 0; pair < half; pair++) {
                Complex w = std::polar(1.0, angle_step * pair);
                int top = group + pair;
                int bot = top + half;
                Complex product = w * x[bot];
                x[bot] = x[top] - product;
                x[top] = x[top] + product;
            }
        }
    }
}

/* CFAR（加角度） */
std::vector<RadarTarget> RadarProcessor::cfar_detect(
    const std::vector<double>& mag,
    const std::vector<double>& phase,
    double angle_deg)
{
    std::vector<RadarTarget> targets;
    int guard = cfar_params_.guard_cells;
    int train = cfar_params_.train_cells;
    int window = guard + train;
    double threshold_factor = std::pow(10.0, cfar_params_.threshold_dB / 20.0);
    int half_n = N_SAMPLES / 2;

    /* 跳过 bin 0-1（DC/近场耦合），限制上界（避免右侧训练格不足造成误报） */
    int i_start = 2;
    int i_end   = half_n - guard - train - 1;   /* = 25，保证两侧训练格完整 */

    for (int i = i_start; i <= i_end; i++) {
        double noise_sum = 0.0;
        int noise_count = 0;
        for (int j = i - window; j <= i - guard - 1; j++) {
            if (j >= 0) { noise_sum += mag[j]; noise_count++; }
        }
        for (int j = i + guard + 1; j <= i + window; j++) {
            if (j < half_n) { noise_sum += mag[j]; noise_count++; }
        }
        if (noise_count == 0) continue;

        double noise_avg = noise_sum / noise_count;
        if (mag[i] > noise_avg * threshold_factor) {
            RadarTarget t;
            t.bin          = i;
            t.range_m      = i * RANGE_RES_M;
            t.amplitude    = mag[i];
            t.velocity_mps = 0.0;
            t.phase_rad    = phase[i];
            t.angle_deg    = angle_deg;
            targets.push_back(t);
        }
    }
    return targets;
}

double RadarProcessor::calc_velocity(double phase_now, double phase_prev, int seq_gap)
{
    double d_phi = phase_now - phase_prev;
    while (d_phi >  M_PI) d_phi -= 2.0 * M_PI;
    while (d_phi < -M_PI) d_phi += 2.0 * M_PI;
    double d_phi_per_frame = d_phi / seq_gap;
    double t_frame = FRAME_MS * 1.0e-3;
    return d_phi_per_frame * C_MPS / (4.0 * M_PI * FC_HZ * t_frame);
}