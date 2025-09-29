# app.py
from flask import Flask, render_template
from flask_socketio import SocketIO, emit
import paho.mqtt.client as mqtt
import json

# ------------------
# Configuración MQTT
# ------------------
MQTT_BROKER = "test.mosquitto.org"
MQTT_PORT = 1883

TOPICS_ESTADO = {1: "riego/estado/1", 2: "riego/estado/2", 3: "riego/estado/3"}
TOPICS_CONTROL = {1: "riego/control/1", 2: "riego/control/2", 3: "riego/control/3"}

# ------------------
# Flask + SocketIO
# ------------------
app = Flask(__name__, static_folder="static", template_folder="templates")
socketio = SocketIO(app, cors_allowed_origins="*")

# Estado más reciente
latest = {1: {}, 2: {}, 3: {}}

# ------------------
# MQTT Callbacks
# ------------------
def on_connect(client, userdata, flags, rc):
    print("[MQTT] Conectado con rc:", rc)
    for t in TOPICS_ESTADO.values():
        client.subscribe(t)
        print("[MQTT] Suscrito a", t)

def on_message(client, userdata, msg):
    topic = msg.topic
    payload_raw = msg.payload.decode(errors="ignore")
    try:
        payload = json.loads(payload_raw)
    except Exception:
        payload = {"raw": payload_raw}

    if topic.startswith("riego/estado/"):
        try:
            tid = int(topic.split("/")[-1])
        except:
            tid = None

        if tid in latest:
            latest[tid] = payload
            # 🔹 Reenviar estado en tiempo real al navegador
            socketio.emit("estado", {"terreno": tid, "payload": payload})
            print(f"[MQTT] Estado recibido terreno {tid}: {payload}")
    else:
        print("[MQTT] Otro topic:", topic, payload)

# ------------------
# Inicializar MQTT
# ------------------
mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start()

# ------------------
# Rutas web
# ------------------
@app.route("/")
def index():
    return render_template("index.html")

# ------------------
# Eventos WebSocket
# ------------------
@socketio.on("connect")
def handle_connect():
    print("[WS] Cliente conectado")
    emit("bulk", latest)

@socketio.on("control")
def handle_control(data):
    tid = data.get("terreno")
    cmd = data.get("cmd")
    if tid in TOPICS_CONTROL and cmd in ("ON", "OFF"):
        topic = TOPICS_CONTROL[tid]
        mqtt_client.publish(topic, cmd)
        print(f"[WS->MQTT] Publish {cmd} to {topic}")
        emit("control_ack", {"terreno": tid, "cmd": cmd})
    else:
        emit("control_ack", {"error": "invalid"})

# ------------------
# Run
# ------------------
if __name__ == "__main__":
    import socket
    local_ip = socket.gethostbyname(socket.gethostname())
    print(f"\n🌍 Abre en navegador:\n  👉 http://127.0.0.1:5000\n  👉 http://{local_ip}:5000 (desde otro dispositivo en tu red)\n")
    socketio.run(app, host="0.0.0.0", port=5000)
