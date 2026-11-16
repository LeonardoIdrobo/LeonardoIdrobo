#pragma once

#include <cstdint>

// Generador de la frecuencia de Schumann.
// La muestra y el PWM viven aquí para que el temporizador del ESP32
// y la prueba de host usen exactamente los mismos números.

constexpr int kOscillators = 5;

constexpr uint32_t kSampleHz = 2000;
constexpr uint64_t kSamplePeriodUs = 1000000ull / kSampleHz;

constexpr uint32_t kPwmFreqHz = 25000;
constexpr uint32_t kPwmBits = 10;
constexpr uint32_t kPwmMaxDuty = (1u << kPwmBits) - 1u;

constexpr uint32_t kAudioToneHz = 880;
constexpr uint32_t kAudioBurstMs = 12;
constexpr uint32_t kAudioBurstSamples = kSampleHz * kAudioBurstMs / 1000u;

constexpr float kSchumannHz = 7.83f;
constexpr float kMinHz = 0.5f;
constexpr float kMaxHz = 20.0f;

// Picos nominales que se citan para la cavidad Tierra-ionosfera.
// En la naturaleza se mueven un poco con la hora y la actividad solar.
constexpr float kHarmonicNominalHz[kOscillators] = {
    7.83f, 14.3f, 20.8f, 27.3f, 33.8f};

// Pesos relativos, más débiles en cada pico sucesivo.
constexpr float kHarmonicAmp[kOscillators] = {
    1.00f, 0.55f, 0.35f, 0.22f, 0.14f};

constexpr uint8_t kModeFundamental = 0;
constexpr uint8_t kModeHarmonics = 1;
constexpr uint8_t kModeAudible = 2;

struct Sample {
  uint32_t coil_duty;  // 0 .. kPwmMaxDuty, senoide que va a la bobina
  uint32_t led_duty;   // 0 o kPwmMaxDuty, un destello por ciclo
  bool audio_on;       // clic audible en el modo 2
};

// Enlace C++ explícito: dentro del firmware de Arduino estas declaraciones
// quedaban con enlace C y el enlazador no encontraba signal.cpp.
#ifdef __cplusplus
extern "C++" {
#endif

void signal_init();
void signal_rewind();

// Devuelve false si hz queda fuera de [kMinHz, kMaxHz] y no cambia nada.
bool signal_set_fundamental(float hz);

// 0 fundamental, 1 fundamental más armónicos, 2 fundamental con clics.
bool signal_set_mode(uint8_t mode);
uint8_t signal_mode();

// Frecuencia real de cada oscilador, ya cuantizada al paso del DDS.
float signal_oscillator_hz(int index);

uint32_t signal_tuning_word(float freq_hz);
Sample signal_next_sample();

#ifdef __cplusplus
}
#endif
