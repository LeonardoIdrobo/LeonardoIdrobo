#include "signal.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int g_fallos = 0;

void fallo(const char* mensaje) {
  std::fprintf(stderr, "FALLO: %s\n", mensaje);
  ++g_fallos;
}

void cerca(const char* nombre, double obtenido, double esperado, double tol) {
  const double delta = std::fabs(obtenido - esperado);
  if (delta > tol) {
    std::fprintf(stderr,
                 "FALLO %s: obtuvo %.6f, esperaba %.6f (tolerancia %.6f)\n",
                 nombre, obtenido, esperado, tol);
    ++g_fallos;
  }
}

struct Medida {
  int cruces;
  double hz;
  uint32_t duty_min;
  uint32_t duty_max;
  int muestras_medias;
  int muestras_led;
};

Medida medir(int muestras) {
  Medida medida{};
  medida.duty_min = kPwmMaxDuty;
  medida.duty_max = 0;
  bool led_antes = false;
  int primero = -1;
  int ultimo = -1;

  for (int i = 0; i < muestras; ++i) {
    const Sample muestra = signal_next_sample();
    if (muestra.coil_duty > kPwmMaxDuty || muestra.led_duty > kPwmMaxDuty) {
      fallo("ciclo de trabajo fuera de rango");
    }
    if (muestra.coil_duty < medida.duty_min) {
      medida.duty_min = muestra.coil_duty;
    }
    if (muestra.coil_duty > medida.duty_max) {
      medida.duty_max = muestra.coil_duty;
    }
    if (muestra.coil_duty > 200 && muestra.coil_duty < 800) {
      ++medida.muestras_medias;
    }
    if (muestra.led_duty != 0 && muestra.led_duty != kPwmMaxDuty) {
      fallo("el LED tiene que ser una onda cuadrada");
    }
    if (muestra.led_duty != 0) {
      ++medida.muestras_led;
    }
    const bool led = muestra.led_duty != 0;
    if (led && !led_antes) {
      if (primero < 0) {
        primero = i;
      }
      ultimo = i;
      ++medida.cruces;
    }
    led_antes = led;
  }

  if (medida.cruces >= 2 && ultimo > primero) {
    medida.hz = (medida.cruces - 1) * static_cast<double>(kSampleHz) /
                static_cast<double>(ultimo - primero);
  }
  return medida;
}

void prueba_palabra_de_sintonia() {
  const uint32_t palabra = signal_tuning_word(7.83f);
  const double vuelta =
      static_cast<double>(palabra) * kSampleHz / 4294967296.0;
  cerca("cuantizacion 7.83", vuelta, 7.83, 0.00002);

  const uint32_t lenta = signal_tuning_word(0.5f);
  const uint32_t rapida = signal_tuning_word(20.0f);
  if (!(lenta < palabra && palabra < rapida)) {
    fallo("la palabra de sintonia no crece con la frecuencia");
  }
}

void prueba_frecuencias_nominales() {
  signal_init();
  if (!signal_set_fundamental(kSchumannHz)) {
    fallo("la fundamental nominal fue rechazada");
  }
  cerca("oscilador 0", signal_oscillator_hz(0), 7.83, 0.0001);
  for (int i = 1; i < kOscillators; ++i) {
    const double razon =
        static_cast<double>(kHarmonicNominalHz[i]) / kHarmonicNominalHz[0];
    const double esperado = 7.83 * razon;
    char nombre[32];
    std::snprintf(nombre, sizeof(nombre), "oscilador %d", i);
    cerca(nombre, signal_oscillator_hz(i), esperado, 0.0001);
  }
}

void prueba_limites() {
  signal_init();
  const float antes = signal_oscillator_hz(0);
  if (signal_set_fundamental(0.2f) || signal_set_fundamental(40.0f)) {
    fallo("acepto una frecuencia fuera de rango");
  }
  cerca("frecuencia intacta", signal_oscillator_hz(0), antes, 1e-6);
  if (!signal_set_fundamental(kMinHz) || !signal_set_fundamental(kMaxHz)) {
    fallo("rechazo un extremo valido del rango");
  }
  if (signal_set_mode(9)) {
    fallo("acepto un modo inexistente");
  }
  if (signal_mode() != kModeFundamental) {
    fallo("un modo invalido cambio el modo activo");
  }
  if (!signal_set_mode(kModeAudible) || signal_mode() != kModeAudible) {
    fallo("no entro en el modo audible");
  }
}

void prueba_senoide() {
  signal_init();
  signal_set_mode(kModeFundamental);
  signal_rewind();
  const int muestras = static_cast<int>(kSampleHz * 10);
  const Medida medida = medir(muestras);
  cerca("frecuencia medida", medida.hz, 7.83, 0.01);
  if (medida.duty_min > 2 || medida.duty_max < kPwmMaxDuty - 2) {
    fallo("la senoide no recorre el fondo de escala del PWM");
  }
  if (medida.muestras_medias < muestras / 5) {
    fallo("la salida de la bobina parece una cuadrada, no una senoide");
  }
  const double fraccion_led =
      static_cast<double>(medida.muestras_led) / muestras;
  cerca("ciclo del LED", fraccion_led, 0.5, 0.05);
}

void prueba_un_hertz() {
  signal_init();
  signal_set_fundamental(1.0f);
  signal_set_mode(kModeFundamental);
  signal_rewind();
  const Medida medida = medir(static_cast<int>(kSampleHz * 5));
  cerca("pulso de prueba a 1 Hz", medida.hz, 1.0, 0.01);
}

void prueba_armonicos_distintos() {
  signal_init();
  signal_set_mode(kModeFundamental);
  signal_rewind();
  const int muestras = static_cast<int>(kSampleHz);
  std::vector<uint32_t> fundamental(muestras);
  for (int i = 0; i < muestras; ++i) {
    fundamental[i] = signal_next_sample().coil_duty;
  }

  signal_set_mode(kModeHarmonics);
  signal_rewind();
  long diferencia = 0;
  uint32_t minimo = kPwmMaxDuty;
  uint32_t maximo = 0;
  for (int i = 0; i < muestras; ++i) {
    const uint32_t duty = signal_next_sample().coil_duty;
    diferencia += std::labs(static_cast<long>(duty) - fundamental[i]);
    if (duty < minimo) {
      minimo = duty;
    }
    if (duty > maximo) {
      maximo = duty;
    }
  }
  if (diferencia < 10000) {
    fallo("el modo con armonicos coincide con la fundamental");
  }
  if (minimo > 200 || maximo < 800 || maximo > kPwmMaxDuty) {
    std::fprintf(stderr, "FALLO: mezcla fuera de una senoide util (min %u max %u)\n",
                 minimo, maximo);
    ++g_fallos;
  }
}

void prueba_clics() {
  signal_init();
  signal_set_mode(kModeAudible);
  signal_rewind();

  const int muestras = static_cast<int>(kSampleHz * 10);
  int rafagas = 0;
  int longitud = 0;
  bool en_rafaga = false;
  bool audio_en_otros_modos = false;

  for (int i = 0; i < muestras; ++i) {
    const bool audio = signal_next_sample().audio_on;
    if (audio && !en_rafaga) {
      en_rafaga = true;
      longitud = 1;
    } else if (audio && en_rafaga) {
      ++longitud;
    } else if (!audio && en_rafaga) {
      en_rafaga = false;
      ++rafagas;
      if (longitud != static_cast<int>(kAudioBurstSamples)) {
        fallo("un clic no dura 12 ms");
      }
    }
  }

  // 10 s a 7,83 Hz. El primer cruce (fase 0) no dispara clic porque la
  // muestra anterior no es negativa; salen los ciclos siguientes.
  if (rafagas < 76 || rafagas > 79) {
    std::fprintf(stderr, "FALLO: rafagas de audio %d\n", rafagas);
    ++g_fallos;
  }

  signal_set_mode(kModeFundamental);
  signal_rewind();
  for (int i = 0; i < static_cast<int>(kSampleHz); ++i) {
    if (signal_next_sample().audio_on) {
      audio_en_otros_modos = true;
    }
  }
  signal_set_mode(kModeHarmonics);
  signal_rewind();
  for (int i = 0; i < static_cast<int>(kSampleHz); ++i) {
    if (signal_next_sample().audio_on) {
      audio_en_otros_modos = true;
    }
  }
  if (audio_en_otros_modos) {
    fallo("hay clics fuera del modo audible");
  }
}

}  // namespace

int main() {
  prueba_palabra_de_sintonia();
  prueba_frecuencias_nominales();
  prueba_limites();
  prueba_senoide();
  prueba_un_hertz();
  prueba_armonicos_distintos();
  prueba_clics();

  if (g_fallos != 0) {
    std::fprintf(stderr, "%d pruebas fallaron\n", g_fallos);
    return 1;
  }
  std::puts("OK: la senal queda en 7,83 Hz, con armonicos y clics de 12 ms.");
  return 0;
}
