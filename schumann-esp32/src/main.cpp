// Generador de la resonancia de Schumann para un ESP32 clásico (WROOM-32).
// En Arduino IDE: copia signal.h, signal.cpp y este archivo a una carpeta
// de sketch, y renombra main.cpp a schumann.ino.

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>

#include "esp_timer.h"
#include "signal.h"

#ifndef PIN_CAMPO
#define PIN_CAMPO 25
#endif
#ifndef PIN_LED
#define PIN_LED 2
#endif
#ifndef PIN_AUDIO
#define PIN_AUDIO 26
#endif
#ifndef PIN_BOTON
#define PIN_BOTON 4
#endif

#if PIN_CAMPO == PIN_LED || PIN_CAMPO == PIN_AUDIO || PIN_CAMPO == PIN_BOTON || \
    PIN_LED == PIN_AUDIO || PIN_LED == PIN_BOTON || PIN_AUDIO == PIN_BOTON
#error Los GPIO del campo, el LED, el audio y el pulsador tienen que ser distintos.
#endif

namespace {

constexpr uint8_t kCanalCampo = 0;
constexpr uint8_t kCanalLed = 1;
constexpr uint8_t kCanalAudio = 2;

bool g_grafica = false;
uint32_t g_muestra_impresa = 0;
std::atomic<uint32_t> g_muestras{0};
std::atomic<uint32_t> g_ultimo_duty{0};

esp_timer_handle_t g_timer = nullptr;

bool g_boton_estable = true;
bool g_boton_ultima = true;
uint32_t g_boton_cambio_ms = 0;

void pwm_begin(uint8_t pin, uint8_t channel, uint32_t freq) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)channel;
  ledcAttach(pin, freq, kPwmBits);
#else
  ledcSetup(channel, freq, kPwmBits);
  ledcAttachPin(pin, channel);
#endif
}

void pwm_write(uint8_t pin, uint8_t channel, uint32_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)channel;
  ledcWrite(pin, duty);
#else
  (void)pin;
  ledcWrite(channel, duty);
#endif
}

const char* nombre_modo(uint8_t mode) {
  switch (mode) {
    case kModeHarmonics:
      return "armónicos";
    case kModeAudible:
      return "fundamental + clics";
    default:
      return "fundamental";
  }
}

void imprimir_ayuda() {
  Serial.println();
  Serial.println("Generador de la resonancia de Schumann");
  Serial.println("Señal local a 7,83 Hz para bobina, LED y zumbador.");
  Serial.println();
  Serial.println("  f <Hz>      fundamental entre 0.5 y 20");
  Serial.println("  modo <0-2>  0 senoide, 1 con armónicos, 2 senoide + clics");
  Serial.println("  estado      frecuencias que está generando");
  Serial.println("  grafica     datos para el Serial Plotter");
  Serial.println("  ayuda       esta lista");
  Serial.println();
  Serial.println("f 1 hace un pulso lento para probar el cableado.");
  Serial.println("Luego vuelve con f 7.83");
  Serial.println();
}

void imprimir_estado() {
  const uint8_t mode = signal_mode();
  Serial.println();
  Serial.printf("Modo %u (%s)\n", mode, nombre_modo(mode));
  const int voces = (mode == kModeHarmonics) ? kOscillators : 1;
  for (int i = 0; i < voces; ++i) {
    Serial.printf("  oscilador %d: %.4f Hz\n", i, signal_oscillator_hz(i));
  }
  Serial.printf("Muestra %lu Hz, PWM %lu Hz, clic %lu Hz\n",
                static_cast<unsigned long>(kSampleHz),
                static_cast<unsigned long>(kPwmFreqHz),
                static_cast<unsigned long>(kAudioToneHz));
}

void manejar_linea(char* line) {
  while (*line == ' ' || *line == '\t') {
    ++line;
  }
  if (*line == '\0') {
    return;
  }

  char* arg = line;
  while (*arg != '\0' && *arg != ' ' && *arg != '\t') {
    if (*arg >= 'A' && *arg <= 'Z') {
      *arg = static_cast<char>(*arg - 'A' + 'a');
    }
    ++arg;
  }
  if (*arg != '\0') {
    *arg = '\0';
    ++arg;
    while (*arg == ' ' || *arg == '\t') {
      ++arg;
    }
  } else {
    arg = nullptr;
  }

  if (strcmp(line, "ayuda") == 0 || strcmp(line, "help") == 0 ||
      strcmp(line, "?") == 0) {
    imprimir_ayuda();
    return;
  }
  if (strcmp(line, "estado") == 0) {
    imprimir_estado();
    return;
  }
  if (strcmp(line, "grafica") == 0) {
    g_grafica = !g_grafica;
    g_muestra_impresa = g_muestras.load(std::memory_order_relaxed);
    Serial.println(g_grafica ? "Gráfica activada. Abre el Serial Plotter."
                             : "Gráfica en silencio.");
    return;
  }
  if (strcmp(line, "modo") == 0) {
    if (arg == nullptr) {
      Serial.println("Usa modo 0, modo 1 o modo 2.");
      return;
    }
    char* fin = nullptr;
    const unsigned long valor = strtoul(arg, &fin, 10);
    if (fin == arg || *fin != '\0' || valor > kModeAudible) {
      Serial.println("Modo inválido. Usa 0, 1 o 2.");
      return;
    }
    signal_set_mode(static_cast<uint8_t>(valor));
    imprimir_estado();
    return;
  }
  if (strcmp(line, "f") == 0) {
    if (arg == nullptr || *arg == '\0') {
      Serial.printf("Fundamental %.4f Hz\n", signal_oscillator_hz(0));
      return;
    }
    char* end = nullptr;
    const float hz = strtof(arg, &end);
    if (end == arg || *end != '\0') {
      Serial.println("No entendí el número. Ejemplo: f 7.83");
      return;
    }
    if (!signal_set_fundamental(hz)) {
      Serial.printf("Fuera de rango. Usa un valor entre %.1f y %.1f Hz.\n",
                    kMinHz, kMaxHz);
      return;
    }
    imprimir_estado();
    return;
  }

  Serial.println("Comando desconocido. Escribe ayuda.");
}

void leer_serie() {
  static char buf[48];
  static size_t len = 0;
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c != '\n') {
      if (len + 1 < sizeof(buf)) {
        buf[len++] = c;
      }
      continue;
    }
    buf[len] = '\0';
    len = 0;
    manejar_linea(buf);
  }
}

void leer_boton() {
  const bool lectura = digitalRead(PIN_BOTON) == HIGH;
  if (lectura != g_boton_ultima) {
    g_boton_cambio_ms = millis();
    g_boton_ultima = lectura;
  }
  if ((millis() - g_boton_cambio_ms) < 40 || lectura == g_boton_estable) {
    return;
  }
  g_boton_estable = lectura;
  if (g_boton_estable) {
    return;
  }
  const uint8_t siguiente =
      static_cast<uint8_t>((signal_mode() + 1) % (kModeAudible + 1));
  signal_set_mode(siguiente);
  imprimir_estado();
}

void pintar_grafica() {
  if (!g_grafica) {
    return;
  }
  const uint32_t ahora = g_muestras.load(std::memory_order_relaxed);
  if (ahora - g_muestra_impresa < 10) {
    return;
  }
  g_muestra_impresa = ahora;
  Serial.println(g_ultimo_duty.load(std::memory_order_relaxed));
}

void on_sample(void* /*arg*/) {
  const Sample sample = signal_next_sample();
  g_ultimo_duty.store(sample.coil_duty, std::memory_order_relaxed);
  g_muestras.fetch_add(1, std::memory_order_relaxed);
  pwm_write(PIN_CAMPO, kCanalCampo, sample.coil_duty);

  uint32_t led = sample.led_duty;
#ifdef LED_ACTIVO_BAJO
  led = kPwmMaxDuty - led;
#endif
  pwm_write(PIN_LED, kCanalLed, led);
  pwm_write(PIN_AUDIO, kCanalAudio, sample.audio_on ? (kPwmMaxDuty / 2) : 0);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  signal_init();

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pwm_begin(PIN_CAMPO, kCanalCampo, kPwmFreqHz);
  pwm_begin(PIN_LED, kCanalLed, kPwmFreqHz);
  pwm_begin(PIN_AUDIO, kCanalAudio, kAudioToneHz);
  pwm_write(PIN_CAMPO, kCanalCampo, 0);
  pwm_write(PIN_LED, kCanalLed, 0);
  pwm_write(PIN_AUDIO, kCanalAudio, 0);

  esp_timer_create_args_t args = {};
  args.callback = &on_sample;
  args.arg = nullptr;
  args.dispatch_method = ESP_TIMER_TASK;
  args.name = "schumann";
  args.skip_unhandled_events = true;
  esp_err_t err = esp_timer_create(&args, &g_timer);
  if (err == ESP_OK) {
    err = esp_timer_start_periodic(g_timer, kSamplePeriodUs);
  }
  if (err != ESP_OK) {
    Serial.printf("No se pudo arrancar el temporizador (%d).\n",
                  static_cast<int>(err));
    return;
  }

  imprimir_ayuda();
  imprimir_estado();
  Serial.printf("Pulsador en GPIO%d: cambia de modo.\n", PIN_BOTON);
  Serial.printf("Bobina en GPIO%d, LED en GPIO%d, zumbador en GPIO%d.\n",
                PIN_CAMPO, PIN_LED, PIN_AUDIO);
}

void loop() {
  leer_serie();
  leer_boton();
  pintar_grafica();
}
