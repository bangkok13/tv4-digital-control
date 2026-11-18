#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace tv4 {
// One observation per elapsed second; -1 is an unobserved second.
class RpmHistory {
  public:
    static constexpr unsigned capacity = 600, columns = 60;
    struct View {
        std::array<int16_t, columns> points{};
        unsigned width = 0, count = 0, minimum = 0, maximum = 0;
        double mean = 0;
    };
    RpmHistory() { samples_.fill(-1); }
    void sample(uint64_t us, bool valid, double rpm) {
        const auto sec = us / 1000000;
        if (started_ && sec <= last_)
            return;
        if (!started_ || sec - last_ >= capacity)
            samples_.fill(-1);
        else
            for (auto i = last_ + 1; i < sec; ++i)
                samples_[i % capacity] = -1;
        samples_[sec % capacity] =
            valid && std::isfinite(rpm) && rpm >= 0 && rpm <= 6000 ? int16_t(std::lround(rpm)) : -1;
        last_ = sec;
        started_ = true;
    }
    View view(unsigned seconds, uint64_t nowUs) const {
        View v;
        v.points.fill(-1);
        seconds = std::clamp(seconds, 1u, capacity);
        v.width = std::min(seconds, columns);
        const uint64_t end = nowUs / 1000000;
        unsigned sums[columns]{}, counts[columns]{}, total = 0;
        for (unsigned i = 0; i < seconds; ++i) {
            const unsigned age = seconds - 1 - i;
            if (!started_ || age > end)
                continue;
            const uint64_t t = end - age;
            if (t > last_ || last_ - t >= capacity)
                continue;
            const int p = samples_[t % capacity];
            if (p < 0)
                continue;
            unsigned col = i * v.width / seconds;
            sums[col] += p;
            ++counts[col];
            total += p;
            if (!v.count || unsigned(p) < v.minimum)
                v.minimum = p;
            v.maximum = std::max(v.maximum, unsigned(p));
            ++v.count;
        }
        for (unsigned i = 0; i < columns; ++i)
            if (counts[i])
                v.points[i] = (sums[i] + counts[i] / 2) / counts[i];
        v.mean = v.count ? double(total) / v.count : 0;
        return v;
    }

  private:
    std::array<int16_t, capacity> samples_{};
    uint64_t last_ = 0;
    bool started_ = false;
};
} // namespace tv4
