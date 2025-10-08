#include <WiFi.h>
#include <PubSubClient.h>

// Configurações Wi-Fi
const char* ssid = "";
const char* password = "";

// Configurações MQTT
const char* mqtt_server = ""; 
const int mqtt_port = 1883;
const char* mqtt_topic = "sensor/pressao";

WiFiClient espClient;
PubSubClient client(espClient);

// Pino do ADC
const int adcPin = 34; // Pino onde o sinal do sensor está conectado

void setup() {
  Serial.begin(115200);

  // Conecta ao Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conectado ao Wi-Fi");

  // Configura o MQTT
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  // Lê a tensão no pino ADC
  int adcValue = analogRead(adcPin);
  float voltage = (adcValue / 4095.0) * 3.3; // Converte para tensão (0-3,3V)

// Converte a tensão para corrente (mA)
  float current = (voltage / 250.0) * 1000; // V = I * R => I = V / R

// Converte a corrente para pressão (bar)
  float pressure = ((current - 4) / 16.0) * 10; // 4 mA = 0 bar, 20 mA = 10 bar
  // Exibe os valores no Serial Monitor
  Serial.print("Tensão: ");
  Serial.print(voltage);
  Serial.print(" V | Corrente: ");
  Serial.print(current);
  Serial.print(" mA | Pressão: ");
  Serial.print(pressure);
  Serial.println(" bar");

  // Publica a pressão no MQTT
  char payload[50];
  snprintf(payload, 50, "%.2f", pressure);
  client.publish(mqtt_topic, payload);

  delay(1000); // Aguarda 1 segundo antes de ler novamente
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}
