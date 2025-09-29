# ⚡ Sistema de Riego Automático – Firmware ESP32

Este módulo corresponde al **código del ESP32** desarrollado y probado en **Wokwi**.  
Su función es la de **medir variables ambientales y controlar el riego** de tres terrenos de cultivo.  

La comunicación se realiza con el **broker público de MQTT** `test.mosquitto.org`, enviando datos de estado y recibiendo órdenes de control desde la aplicación web.

---

## 📂 1. Estructura del módulo

- `sketch.ino` → Código principal del ESP32 (Arduino framework, probado en Wokwi).  
- `diagram.json` → Diagrama de conexión en Wokwi.  
- `README_ESP32.md` → Este documento.  

---

## 🚀 2. Librerías necesarias

Antes de cargar el código en el ESP32 debes instalar las siguientes librerías en el **Arduino IDE** o **PlatformIO**:

- [**DHT sensor library**](https://github.com/adafruit/DHT-sensor-library)  
- [**PubSubClient**](https://pubsubclient.knolleary.net/)  
- [**WiFi (ESP32)**](https://github.com/espressif/arduino-esp32)

---

## 🔧 3. Instalación y ejecución

### 3.1 Ejecución en Arduino IDE
1. Instalar el **board ESP32** en Arduino IDE (`ESP32 by Espressif Systems`).  
2. Instalar las librerías mencionadas en la sección anterior.  
3. Copiar el contenido de `sketch.ino` en un nuevo proyecto de Arduino IDE.  
4. Seleccionar la placa **ESP32 DevKit v1**.  
5. Cargar el código al ESP32 mediante USB.  

### 3.2 Ejecución en Wokwi
1. Abrir [https://wokwi.com](https://wokwi.com).  
2. Crear un nuevo proyecto con ESP32.  
3. Sustituir el archivo `diagram.json` con el incluido en este repositorio.  
4. Sustituir el archivo `sketch.ino` con el código del firmware.  
5. Ejecutar la simulación y observar los datos en el **Serial Monitor** y en la **aplicación web** conectada vía MQTT.  

---

## 🌐 4. Flujo de comunicación MQTT

El firmware envía y recibe mensajes MQTT para cada terreno (`1`, `2`, `3`):

### Publicación de estado
- `riego/estado/1`  
- `riego/estado/2`  
- `riego/estado/3`  

El mensaje JSON incluye:  

```json
{
  "temp": 23.5,
  "humAmb": 45.0,
  "humSuelo": 62.0,
  "volumen": 3.2,
  "bomba": "ON"
}
```

### Control remoto (suscripción)
- `riego/control/1`  
- `riego/control/2`  
- `riego/control/3`  

Mensajes aceptados:  
- `"ON"` → Encender bomba manualmente.  
- `"OFF"` → Apagar bomba manualmente.  

---

## 🛑 5. Lógicas de seguridad implementadas

El firmware incluye mecanismos para proteger el sistema:  
- Corte automático si el riego excede **30 minutos** (`MAX_RIEGO_MS`).  
- Corte automático si se supera el **volumen objetivo acumulado**.  
- Encendido automático si la humedad del suelo baja de `HUM_ON = 60%`.  
- Apagado automático si la humedad supera `HUM_OFF = 80%`.  
- Alerta si la **temperatura** está fuera del rango [10°C – 30°C].  

---

## 📊 6. Diagrama de simulación (Wokwi)

El archivo `diagram.json` contiene todas las conexiones simuladas en Wokwi.  
Para abrirlo basta con pegarlo en el editor de [wokwi.com](https://wokwi.com).  

---

## ✅ 7. Resumen

Con este firmware, el ESP32:  
- Lee temperatura y humedad ambiente con **DHT22**.  
- Lee humedad del suelo con **potenciómetros** (simulados).  
- Controla las **bombas de riego** mediante **módulos relay**.  
- Publica datos en tiempo real vía **MQTT**.  
- Recibe órdenes de control desde la **aplicación web Flask**.  

---
