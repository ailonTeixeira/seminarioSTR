// REQUISITO DE TEMPO REAL: Inclusão das bibliotecas do FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <WiFi.h>
#include <PubSubClient.h>

// --- CONFIGURAÇÕES DO PROJETO (AJUSTADAS PARA WOKWI) ---

// Wi-Fi (Padrão do Wokwi)
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT (Broker público para simulação)
const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 1883;
// MESMO tópico que o ESP32 sensor está publicando
const char* mqtt_topic = "wokwi/pressao/pressureMQTT"; 

// Hardware de Atuação
const int RELAY_PIN = 2; // Pino GPIO conectado ao relé. No Wokwi, é o LED azul.

// --- LÓGICA DE CONTROLE --
#define PRESSAO_LIGA    7.0
#define PRESSAO_DESLIGA 9.0

// --- CONFIGURAÇÕES DE TEMPO REAL ---
#define CONTROL_TASK_PRIORITY 2 // Prioridade mais alta para a tarefa de controle
#define MQTT_TASK_PRIORITY    1 // Prioridade mais baixa para a tarefa de rede

// --- OBJETOS GLOBAIS ---
WiFiClient espClient;
PubSubClient client(espClient);

// REQUISITO DE TEMPO REAL: Fila para passar dados da callback do MQTT para a tarefa de controle
QueueHandle_t pressureDataQueue;

// --- DECLARAÇÃO DAS TAREFAS E CALLBACKS ---
void taskControlLogic(void *parameter);
void taskMqttHandler(void *parameter);
void mqttCallback(char* topic, byte* payload, unsigned int length);

// ================================================================= //
//                            SETUP                                  //
// ================================================================= //
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configura o pino do relé como saída
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Garante que o compressor comece desligado

  // Conecta ao Wi-Fi
  Serial.print("Atuador conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado ao Wi-Fi!");

  // Configura o MQTT e a função de callback que será chamada ao receber mensagens
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  // Cria a fila para receber os dados de pressão
  pressureDataQueue = xQueueCreate(5, sizeof(float));

  // Cria as tarefas do FreeRTOS
  xTaskCreatePinnedToCore(taskControlLogic, "Control Task", 2048, NULL, CONTROL_TASK_PRIORITY, NULL, 1);
  xTaskCreatePinnedToCore(taskMqttHandler, "MQTT Task", 4096, NULL, MQTT_TASK_PRIORITY, NULL, 0);

  Serial.println("Sistema de tempo real do ATUADOR iniciado.");
}

// ================================================================= //
//          CALLBACK DO MQTT (Executa em um contexto de interrupção) //
// ================================================================= //
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // REQUISITO DE TEMPO REAL: A callback deve ser o mais rápida possível.
  // Ela apenas converte o dado e o envia para a fila, sem fazer lógica complexa.
  
  // Converte o payload (bytes) para uma string e depois para float
  payload[length] = '\0'; // Adiciona terminador nulo
  float pressure = atof((char*)payload);
  
  // Envia o dado para a fila para ser processado pela tarefa de controle
  xQueueSend(pressureDataQueue, &pressure, 0);
}

// ================================================================= //
//               TAREFA DE CONTROLE (ALTA PRIORIDADE)                //
// ================================================================= //
void taskControlLogic(void *parameter) {
  Serial.println("Tarefa de Controle (Prio Alta) iniciada. Aguardando dados de pressão...");
  float receivedPressure;

  for (;;) {
    // REQUISITO DE TEMPO REAL: A tarefa "dorme" aqui (sem consumir CPU) até um
    // novo dado chegar na fila. portMAX_DELAY significa esperar indefinidamente.
    if (xQueueReceive(pressureDataQueue, &receivedPressure, portMAX_DELAY) == pdTRUE) {
      Serial.print("[Control Task] Pressão recebida: ");
      Serial.print(receivedPressure);
      Serial.println(" bar");

      // --- LÓGICA DE CONTROLE PRINCIPAL ---
      if (receivedPressure <= PRESSAO_LIGA) {
        if (digitalRead(RELAY_PIN) == LOW) {
          Serial.println("  -> AÇÃO: Ligando o relé do compressor.");
          digitalWrite(RELAY_PIN, HIGH); // Liga o compressor
        }
      } else if (receivedPressure >= PRESSAO_DESLIGA) {
        if (digitalRead(RELAY_PIN) == HIGH) {
          Serial.println("  -> AÇÃO: Desligando o relé do compressor.");
          digitalWrite(RELAY_PIN, LOW); // Desliga o compressor
        }
      }
    }
  }
}

// ================================================================= //
//              TAREFA DE REDE/MQTT (BAIXA PRIORIDADE)               //
// ================================================================= //
void taskMqttHandler(void *parameter) {
  Serial.println("Tarefa MQTT (Prio Baixa) iniciada.");
  for (;;) {
    if (!client.connected()) {
      Serial.print("MQTT desconectado. Reconectando e assinando tópico...");
      if (client.connect("ESP32_ActuatorClient")) {
        Serial.println("Conectado!");
        // REQUISITO DE TEMPO REAL: Re-assina o tópico após reconectar
        client.subscribe(mqtt_topic);
      } else {
        Serial.println("Falha na reconexão.");
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
    }
    // Mantém a conexão MQTT viva e processa mensagens recebidas
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// ================================================================= //
//                        LOOP PRINCIPAL (VAZIO)                     //
// ================================================================= //
void loop() {
  // O loop principal fica vazio. O FreeRTOS agora gerencia a execução.
}
