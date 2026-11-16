#include "signal.h"

#include <atomic>
#include <cmath>

namespace {

constexpr double kPhaseScale = 4294967296.0;

struct State {
  int16_t lut[256]{};
  uint32_t phase[kOscillators]{};
  std::atomic<uint32_t> tuning[kOscillators];
  std::atomic<uint8_t> mode{kModeFundamental};
  int16_t prev_fundamental = 0;
  uint16_t audio_left = 0;
  bool ready = false;
};

State g;

static_assert(kSamplePeriodUs == 500, "2 kHz equivalen a 500 us");
static_assert(kAudioBurstSamples == 24, "cada clic dura 12 ms");

int16_t sine_at(uint32_t phase) {
  return g.lut[phase >> 24];
}

uint32_t duty_from_bipolar(int32_t sample) {
  constexpr int32_t kFull = 32767;
  if (sample > kFull) {
    sample = kFull;
  }
  if (sample < -kFull) {
    sample = -kFull;
  }
  const int64_t shifted = static_cast<int64_t>(sample) + kFull;
  return static_cast<uint32_t>((shifted * kPwmMaxDuty) / (2 * kFull));
}

}  // namespace

uint32_t signal_tuning_word(float freq_hz) {
  double word = static_cast<double>(freq_hz) * kPhaseScale /
                static_cast<double>(kSampleHz);
  if (word < 0.0) {
    word = 0.0;
  }
  if (word > kPhaseScale - 1.0) {
    word = kPhaseScale - 1.0;
  }
  return static_cast<uint32_t>(std::llround(word));
}

void signal_rewind() {
  for (int i = 0; i < kOscillators; ++i) {
    g.phase[i] = 0;
  }
  g.prev_fundamental = 0;
  g.audio_left = 0;
}

void signal_init() {
  for (int i = 0; i < 256; ++i) {
    const double angle = (2.0 * 3.14159265358979323846 * i) / 256.0;
    long value = std::lround(std::sin(angle) * 32767.0);
    if (value > 32767) {
      value = 32767;
    }
    if (value < -32767) {
      value = -32767;
    }
    g.lut[i] = static_cast<int16_t>(value);
  }
  g.mode.store(kModeFundamental, std::memory_order_relaxed);
  g.ready = true;
  signal_rewind();
  signal_set_fundamental(kSchumannHz);
}

bool signal_set_fundamental(float hz) {
  if (!(hz >= kMinHz && hz <= kMaxHz)) {
    return false;
  }
  uint32_t words[kOscillators];
  const float base = kHarmonicNominalHz[0];
  for (int i = 0; i < kOscillators; ++i) {
    const float freq = hz * (kHarmonicNominalHz[i] / base);
    words[i] = signal_tuning_word(freq);
  }
  for (int i = 0; i < kOscillators; ++i) {
    g.tuning[i].store(words[i], std::memory_order_relaxed);
  }
  return true;
}

bool signal_set_mode(uint8_t mode) {
  if (mode > kModeAudible) {
    return false;
  }
  g.mode.store(mode, std::memory_order_relaxed);
  return true;
}

uint8_t signal_mode() {
  return g.mode.load(std::memory_order_relaxed);
}

float signal_oscillator_hz(int index) {
  if (index < 0 || index >= kOscillators) {
    return 0.0f;
  }
  const uint32_t word = g.tuning[index].load(std::memory_order_relaxed);
  return static_cast<float>(static_cast<double>(word) *
                            static_cast<double>(kSampleHz) / kPhaseScale);
}

Sample signal_next_sample() {
  if (!g.ready) {
    signal_init();
  }

  const uint8_t mode = g.mode.load(std::memory_order_relaxed);
  int16_t waves[kOscillators];
  for (int i = 0; i < kOscillators; ++i) {
    const uint32_t phase = g.phase[i];
    waves[i] = sine_at(phase);
    const uint32_t step = g.tuning[i].load(std::memory_order_relaxed);
    g.phase[i] = phase + step;
  }

  const int voices = (mode == kModeHarmonics) ? kOscillators : 1;
  double mixed = 0.0;
  double amp_sum = 0.0;
  for (int i = 0; i < voices; ++i) {
    mixed += static_cast<double>(kHarmonicAmp[i]) * waves[i];
    amp_sum += kHarmonicAmp[i];
  }
  int32_t shaped = static_cast<int32_t>(std::llround(mixed / amp_sum));

  const int16_t fundamental = waves[0];
  const bool rising = (g.prev_fundamental < 0 && fundamental >= 0);
  g.prev_fundamental = fundamental;

  bool audio = false;
  if (mode == kModeAudible) {
    if (rising) {
      g.audio_left = static_cast<uint16_t>(kAudioBurstSamples);
    }
    if (g.audio_left > 0) {
      audio = true;
      --g.audio_left;
    }
  } else {
    g.audio_left = 0;
  }

  Sample sample;
  sample.coil_duty = duty_from_bipolar(shaped);
  sample.led_duty = (fundamental >= 0) ? kPwmMaxDuty : 0;
  sample.audio_on = audio;
  return sample;
}
