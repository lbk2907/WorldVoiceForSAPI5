// Resampler.h - WorldVoice's rate conversion: a voice's 16-bit mono audio to WorldVoice's rate, with a polyphase
// windowed-sinc filter (Kaiser window, 100 dB stopband), so a converted voice sounds as it does on its own. SAPI's
// own converter was measured (2026-10-11) leaving false tones only 16-39 dB below a tone when converting up, and
// folding sound above the new Nyquist frequency back down at full strength when converting down.
#pragma once
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace wv {

// The filter for one pair of rates: output = input * L / M, with L phases of K taps each.
struct ResampleFilter {
    uint32_t L = 1, M = 1;
    int K = 0;
    std::vector<float> taps;   // L * K: phase p is the filter for an output that falls p/L of a sample after an input

    ResampleFilter(uint32_t inRate, uint32_t outRate) {
        const double kPi = 3.14159265358979323846;
        uint32_t g = Gcd(inRate, outRate);
        L = outRate / g;
        M = inRate / g;
        // The passband ends 5% below the lower rate's Nyquist frequency and the stopband starts at it: an 11025 Hz
        // voice keeps everything up to 5.24 kHz, and the copies of it that converting up would leave (5.51 kHz and
        // up) are 100 dB down; converting down, nothing above the new Nyquist frequency folds back. Frequencies here
        // are in cycles per input sample.
        const double A = 100.0, beta = 0.1102 * (A - 8.7);
        double nyquist = 0.5 * (std::min)(inRate, outRate) / inRate;
        double width = 0.05 * nyquist, cutoff = nyquist - width / 2;
        K = (int)std::ceil((A - 8) / (2.285 * 2 * kPi * width));
        K += K & 1;
        taps.resize((size_t)L * K);
        double i0b = I0(beta), half = K / 2.0;
        for (uint32_t p = 0; p < L; p++) {
            float* h = &taps[(size_t)p * K];
            double sum = 0;
            for (int k = 0; k < K; k++) {
                double t = (double)p / L + (K / 2 - 1) - k;      // input samples from this tap to the output
                double x = t / half;
                double w = std::fabs(x) >= 1 ? 0 : I0(beta * std::sqrt(1 - x * x)) / i0b;
                double s = t == 0 ? 2 * cutoff : std::sin(2 * kPi * cutoff * t) / (kPi * t);
                h[k] = (float)(s * w);
                sum += h[k];
            }
            for (int k = 0; k < K; k++) h[k] = (float)(h[k] / sum);   // every phase passes silence and DC unchanged
        }
    }

    static uint32_t Gcd(uint32_t a, uint32_t b) { while (b) { uint32_t t = a % b; a = b; b = t; } return a ? a : 1; }

    static double I0(double x) {   // the modified Bessel function, for the Kaiser window
        double s = 1, t = 1;
        for (int k = 1; k < 100; k++) {
            t *= (x / (2 * k)) * (x / (2 * k));
            s += t;
            if (t < 1e-12 * s) break;
        }
        return s;
    }
};

// One voice's audio through a filter, as it arrives.
class Resampler {
public:
    explicit Resampler(std::shared_ptr<const ResampleFilter> filter)
        : m_f(std::move(filter)), m_buf((size_t)(m_f->K / 2 - 1), 0.0f), m_start(-(int64_t)(m_f->K / 2 - 1)) {}

    // Takes n input samples and adds the output samples now complete (each needs K/2 inputs after it).
    void Process(const int16_t* in, size_t n, std::vector<int16_t>& out) {
        m_buf.insert(m_buf.end(), in, in + n);
        m_inCount += n;
        Run(out);
    }
    // The end of the audio: the rest of the output, as if silence followed.
    void Flush(std::vector<int16_t>& out) {
        m_buf.insert(m_buf.end(), (size_t)(m_f->K / 2), 0.0f);
        Run(out);
    }
    // Where an input position (in samples) is in the output: the first output sample at or after it.
    uint64_t OutPosition(uint64_t in) const { return (in * m_f->L + m_f->M - 1) / m_f->M; }

private:
    void Run(std::vector<int16_t>& out) {
        const ResampleFilter& f = *m_f;
        const int K = f.K;
        for (;;) {
            uint64_t pos = m_next * f.M;                                   // in 1/L of an input sample
            if (pos >= m_inCount * f.L) break;                             // after the last input
            int64_t first = (int64_t)(pos / f.L) - (K / 2 - 1);            // the first tap's input
            if (first + K > m_start + (int64_t)m_buf.size()) break;        // its inputs are not all here yet
            const float* x = m_buf.data() + (first - m_start);
            const float* h = f.taps.data() + (size_t)(pos % f.L) * K;
            float acc = 0;
            for (int k = 0; k < K; k++) acc += x[k] * h[k];
            long v = lroundf(acc);
            out.push_back((int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v)));
            m_next++;
        }
        // The inputs no output needs any more.
        int64_t keep = (int64_t)(m_next * f.M / f.L) - (K / 2 - 1);
        if (keep - m_start > 8192) {
            m_buf.erase(m_buf.begin(), m_buf.begin() + (size_t)(keep - m_start));
            m_start = keep;
        }
    }

    std::shared_ptr<const ResampleFilter> m_f;
    std::vector<float> m_buf;          // inputs from m_start on (silence before the first)
    int64_t m_start;
    uint64_t m_inCount = 0, m_next = 0;
};

}  // namespace wv
