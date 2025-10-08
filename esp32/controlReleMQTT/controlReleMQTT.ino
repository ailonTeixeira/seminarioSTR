#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";


WiFiClient espClient;
PubSubClient client(espClient);

const int relePin = 5;
String mqttClientId;
bool mqttConnected = false;
unsigned long lastReconnectAttempt = 0;
const long reconnectInterval = 5000;

// Gera um ClientID único baseado no MAC
String generateClientId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String id = "ESP32-";
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 0x10) id += "0";
        id += String(mac[i], HEX);
    }
    return id;
}

void setup_wifi() {
    delay(10);
    Serial.println("\nConectando a " + String(ssid));
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi conectado\nIP: " + WiFi.localIP().toString());
    mqttClientId = generateClientId();
    Serial.println("ClientID: " + mqttClientId);
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Mensagem recebida [");
    Serial.print(topic);
    Serial.print("] ");
    
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println(message);

    if (message == "ligar") {
        digitalWrite(relePin, HIGH);
        Serial.println("Relé ligado");
    } else if (message == "desligar") {
        digitalWrite(relePin, LOW);
        Serial.println("Relé desligado");
    }
}

boolean reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        setup_wifi();
    }
    
    Serial.print("Conectando ao MQTT...");
    if (client.connect(mqttClientId.c_str())) {
        Serial.println("conectado!");
        client.subscribe("casa/rele/controle");
        mqttConnected = true;
        return true;
    }
    Serial.println("falha, rc=" + String(client.state()));
    return false;
}

void setup() {
    pinMode(relePin, OUTPUT);
    digitalWrite(relePin, LOW);
    Serial.begin(115200);
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
    randomSeed(micros());
}

void loop() {
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt >= reconnectInterval) {
            lastReconnectAttempt = now;
            if (reconnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        client.loop();
    }

}
