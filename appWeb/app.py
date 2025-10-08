# -*- coding: utf-8 -*- 
from flask import Flask, render_template, jsonify
import paho.mqtt.client as mqtt
import sqlite3
from datetime import datetime

app = Flask(__name__)

# Variável para armazenar a pressão
pressure = 0.0

# Configurações do MQTT
MQTT_BROKER = "localhost"  # Broker MQTT no Raspberry Pi
MQTT_TOPIC = "sensor/pressao"

# Configuração do banco de dados SQLite
DATABASE = "pressure_data.db"

# Função para criar a tabela no banco de dados
def create_table():
    conn = sqlite3.connect(DATABASE)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS pressure_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            pressure REAL NOT NULL,
            timestamp TEXT NOT NULL
        )
    ''')
    conn.commit()
    conn.close()

# Fução para inserir dados no banco de dados
def insert_data(pressure):
    conn = sqlite3.connect(DATABASE)
    cursor = conn.cursor()
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    cursor.execute('''
        INSERT INTO pressure_readings (pressure, timestamp)
        VALUES (?, ?)
    ''', (pressure, timestamp))
    conn.commit()
    conn.close()

# Função para processar as mensagens MQTT

def on_message(client, userdata, message):
    global pressure 
    payload = message.payload.decode("utf-8")
    print(f"Payload recebido: {payload}")  
    pressure = float(payload)
    print(f"Pressão recebida via MQTT: {pressure} bar")
    insert_data(pressure )

# Configura o cliente MQTT
mqtt_client = mqtt.Client()
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, 1883, 60)
mqtt_client.subscribe(MQTT_TOPIC)
mqtt_client.loop_start()

# Rota da interface web
@app.route("/")
def index():
    return render_template("index.html", pressure=pressure)

# Rota para fornecer os dados em JSON
@app.route("/data")
def data():
    return jsonify({
        "pressure": pressure
    })

# Rota para exibir os dados armazenados
@app.route("/dados")
def dados():
    conn = sqlite3.connect(DATABASE)
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM pressure_readings ORDER BY timestamp DESC")
    rows = cursor.fetchall()
    conn.close()
    return render_template("dados.html", readings=rows)

@app.route("/latest-data")
def latest_data():
    conn = sqlite3.connect(DATABASE)
    cursor = conn.cursor()
    cursor.execute("SELECT pressure, timestamp FROM pressure_readings ORDER BY timestamp DESC LIMIT 20")
    rows = cursor.fetchall()
    conn.close()
    # Retorna os dados em formato JSON
    return jsonify([{
        "pressure": row[0],
        "timestamp": row[1]
    } for row in rows])

if __name__ == "__main__":
    create_table()  # Cria a tabela no banco de dados
    app.run(host="0.0.0.0", port=5000)
