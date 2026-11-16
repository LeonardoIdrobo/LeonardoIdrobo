# Generador de la resonancia de Schumann (ESP32)

Un ESP32 DevKit produce una señal local a **7,83 Hz**, la fundamental de la
resonancia de Schumann, y puede sumar los picos que suelen citarse junto a ella:
14,3 Hz, 20,8 Hz, 27,3 Hz y 33,8 Hz.

La bobina recibe una senoide. El LED de la placa marca cada ciclo. Un zumbador
opcional hace un clic corto en cada ciclo, porque 7,83 Hz está por debajo de lo
que el oído reconoce como un tono.

La señal es de banco de pruebas. La resonancia de Schumann real es un modo
electromagnético de la cavidad entre la superficie de la Tierra y la ionosfera,
alimentado por los rayos. Este circuito genera esa frecuencia aquí, en la
protoboard, con un campo magnético débil junto a la bobina.

## Primer encendido

Graba el programa con la placa sola, sin bobina. El LED de GPIO2 ya demuestra
la frecuencia. Desconecta el USB antes de pinchar la protoboard.

## Qué se ve al encenderlo

1. El monitor serie abre a 115200 baudios y muestra la ayuda.
2. `f 1` deja el LED latiendo una vez por segundo. Sirve para confirmar el cableado.
3. `f 7.83` vuelve a la fundamental. El LED parpadea unas ocho veces por segundo.
4. `grafica` imprime el ciclo de trabajo. El Serial Plotter de Arduino dibuja la senoide.
5. El pulsador pasa del modo 0 al 1 y al 2.

| Modo | Bobina | LED | Zumbador |
| --- | --- | --- | --- |
| 0 | Senoide a la fundamental | Un destello por ciclo | Silencio |
| 1 | Suma de la fundamental y cuatro armónicos | Sigue la fundamental | Silencio |
| 2 | Senoide a la fundamental | Un destello por ciclo | Clic de 12 ms a 880 Hz en cada ciclo |

La fundamental se puede mover entre 0,5 Hz y 20 Hz. Los armónicos escalan con ella,
así que en 7,83 Hz caen en 14,3, 20,8, 27,3 y 33,8 Hz. Los pesos son 1, 0,55, 0,35,
0,22 y 0,14.

## Material

| Pieza | Cantidad | Notas |
| --- | --- | --- |
| ESP32 DevKit V1 (WROOM-32) | 1 | ESP32 clásico, con GPIO 25 y 26. No vale un S3 ni un C3 sin cambiar pines. |
| Transistor NPN 2N2222 o BC547 | 1 | Conecta por el nombre de cada pata. El orden físico cambia entre modelos. |
| Resistencia 470 Ω | 1 | Base del transistor. |
| Resistencia 100 kΩ | 1 | Mantiene el transistor apagado mientras el ESP32 arranca. |
| Resistencia 100 Ω, ½ W | 1 | Limita la corriente de la bobina. |
| Diodo 1N4007 o 1N4148 | 1 | Diodo volante de la bobina. |
| Inductor de 10 mH o más, o bobina de relé de 5 V | 1 | También sirven 50–200 vueltas de alambre esmaltado sobre un tornillo. |
| Pulsador | 1 | Opcional. También puedes cambiar de modo por el monitor serie. |
| Zumbador pasivo | 1 | Opcional, para el modo 2. |
| Resistencia 220 Ω | 1 | En serie con el zumbador. |
| Protoboard y cables |  | |

El LED de GPIO2 de la mayoría de DevKit ya sirve. Si la tuya no tiene, añade un LED
y una resistencia de 330 Ω.

## Cableado

Mira el transistor con la hoja de datos del modelo exacto que compres. En un
PN2222 el orden de las patas no es el de un BC547. Aquí se nombran emisor, base
y colector, no izquierda y derecha.

```text
                         5V del DevKit (USB)
                          |
                       [100 Ω, ½ W]
                          |
                          +------- cátodo del 1N4007 (la raya)
                          |            |
                        bobina         |
                          |            |
                          +------- ánodo del 1N4007
                          |
                       colector
                          |
                       NPN 2N2222 / BC547
                          |
                       emisor ---------------- GND del ESP32
                          |
                        base
                       /    \
                 [470 Ω]   [100 kΩ]
                    |         |
                 GPIO25      GND


GPIO2   LED de la placa. Si añades uno externo:
        GPIO2 -- [330 Ω] -- ánodo del LED -- cátodo -- GND

GPIO26  zumbador pasivo, opcional:
        GPIO26 -- [220 Ω] -- zumbador -- GND
        Si el zumbador trae marca +, ese lado va hacia el GPIO.

GPIO4   pulsador a GND. El pull-up es interno.
        Al pulsarlo se avanza el modo: 0 → 1 → 2 → 0.

GND     común de emisor, diodo, pulsador, LED y zumbador.
```

GPIO25 no alimenta la bobina. Solo mueve la base del transistor. Durante el
arranque, la resistencia de 100 kΩ mantiene esa base a masa para que la bobina
no reciba corriente mientras el pin todavía no es una salida.

La bobina sale del pin **5V** del DevKit, el que viene del USB. Así no carga el
regulador de 3,3 V.

Con la resistencia de 100 Ω y una bobina de poca resistencia propia, el pico de
corriente queda cerca de:

```text
I_pico = (5 V − 0,2 V) / (100 Ω + Rbobina) ≈ 48 mA
```

La senoide recorre desde casi cero hasta ese pico, así que la corriente media
anda por la mitad. La suma de las dos resistencias tiene que seguir por encima
de 96 Ω. El 2N2222 aguanta ese pico. En el pico, la resistencia de 100 Ω disipa
cerca de ¼ W, por eso se pide de ½ W. El inductor tiene que aguantar 50 mA.

## Grabar el programa

Hace falta [PlatformIO](https://platformio.org/). El proyecto pide la plataforma
`espressif32` 8.0.0, que es con la que se compiló el firmware. Desde esta carpeta:

```bash
pio run -t upload
pio device monitor
```

La placa del proyecto es `esp32dev`. Mantén pulsado BOOT si la grabación no
engancha sola; suéltalo cuando aparezca `Connecting`.

En Arduino IDE también funciona: crea un sketch, copia `src/signal.h`,
`src/signal.cpp` y `src/main.cpp`, y renombra `main.cpp` a `schumann.ino`.
Placa «ESP32 Dev Module», 115200 baudios. Sirve Arduino-ESP32 2.x y 3.x.

### Comandos del monitor

```text
f 7.83      fundamental
f 1         pulso lento de prueba
modo 0      solo la senoide
modo 1      fundamental y armónicos
modo 2      senoide y clics
estado      frecuencias que están saliendo
grafica     activa o calla el Serial Plotter
ayuda
```

El punto decimal es el punto, no la coma.

## Cómo se fabrica la senoide

Un temporizador toma 2000 muestras por segundo. En cada muestra un oscilador
digital (DDS de 32 bits) avanza la fase y lee una tabla de seno. Con 7,83 Hz la
frecuencia real, después de redondear la palabra de sintonía, se aparta menos
de 0,00002 Hz.

Esa senoide no sale bit a bit por el pin. El pin lleva un PWM de 25 kHz y 10 bits,
y la senoide es el ciclo de trabajo. La bobina y la resistencia de 100 Ω promedian
el PWM: la inductancia frena la portadora de 25 kHz y la corriente sigue los 7,83 Hz.
El transistor conduce en un solo sentido, así que esa senoide va montada sobre una
corriente media. El LED está a tope la mitad positiva del ciclo y apagado la otra
mitad, para que el parpadeo se pueda contar.

## Límites

- El campo magnético existe junto a la bobina y es débil, del orden de la corriente
  que fija la resistencia de 100 Ω.
- 7,83 Hz no se oye. El modo 2 solo marca el ritmo con clics.
- El firmware es un generador de señal. No tiene una función terapéutica.
- Los picos naturales se mueven un poco con la hora del día y con el Sol. Aquí
  están fijos en los valores nominales, o en el valor que escribas con `f`.

## Probar la señal sin la placa

La síntesis está separada del ESP32. En un ordenador:

```bash
make test
```

Eso comprueba la cuantización a 7,83 Hz, los cuatro armónicos, el rechazo de
frecuencias fuera de 0,5–20 Hz, el pulso de 1 Hz, que el modo 1 no repite la
fundamental y que cada clic dura 12 ms.
