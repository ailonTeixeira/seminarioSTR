// REQUISITO DE TEMPO REAL: Inclusão das bibliotecas do FreeRTOS
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <WiFi.h>
#include <PubSubClient.h>


// Wi-Fi
const char* ssid = "";
const char* password = "";

// MQTT
const char* mqtt_server = ""; 
const int mqtt_port = 1883;
const char* mqtt_topic = ""; 

// --- ARQUITETURA DE HARDWARE ---
#define NUM_COMPRESSORS 4
// Mapeia os pinos GPIO para cada relé de compressor
const int RELAY_PINS[NUM_COMPRESSORS] = {2, 4, 5, 18}; 

// --- LÓGICA DE CONTROLE INTELIGENTE ---
#define PRESSAO_LIGA_COMPRESSOR   7.0
#define PRESSAO_DESLIGA_COMPRESSOR 9.0

// --- CONFIGURAÇÕES DE TEMPO REAL ---
#define CONTROL_TASK_PRIORITY 2 
#define MQTT_TASK_PRIORITY    1

// --- CONFIGURAÇÕES DE REDE ---
#define MQTT_KEEPALIVE 60
#define MQTT_SOCKET_TIMEOUT 30
#define MAX_RECONNECT_DELAY 30000 // 30 segundos máximo
#define INITIAL_RECONNECT_DELAY 1000 // 1 segundo inicial

// --- OBJETOS E VARIÁVEIS GLOBAIS ---
WiFiClient espClient;
PubSubClient client(espClient);
QueueHandle_t pressureDataQueue;

// Variáveis para gerenciar o rodízio e o tempo de operação dos compressores
int nextCompressorToTurnOn = 0; // Índice do próximo compressor a ser ligado
unsigned long compressorOnTime[NUM_COMPRESSORS] = {0}; // Armazena quando cada compressor foi ligado

// --- DECLARAÇÃO DAS TAREFAS E CALLBACKS ---
void taskControlLogic(void *parameter);
void taskMqttHandler(void *parameter);
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool reconnectMQTT(int &reconnectAttempts);

// ================================================================= //
//                            SETUP                                  //
// ================================================================= //
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configura todos os pinos de relé como saída e os desliga
  Serial.println("Configurando pinos dos compressores...");
  for (int i = 0; i < NUM_COMPRESSORS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }

  // Conexão Wi-Fi
  Serial.print("Atuador conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  
  // REQUISITO DE TEMPO REAL: Timeout na conexão WiFi
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiTimeout) < 15000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado ao Wi-Fi!");
    Serial.printf("IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\nFalha na conexão WiFi!");
  }

  // Configuração MQTT com timeouts
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setKeepAlive(MQTT_KEEPALIVE);
  client.setSocketTimeout(MQTT_SOCKET_TIMEOUT);

  // Criação da Fila
  pressureDataQueue = xQueueCreate(5, sizeof(float));

  // Criação das Tarefas
  xTaskCreatePinnedToCore(taskControlLogic, "Control Task", 4096, NULL, CONTROL_TASK_PRIORITY, NULL, 1);
  xTaskCreatePinnedToCore(taskMqttHandler, "MQTT Task", 4096, NULL, MQTT_TASK_PRIORITY, NULL, 0);

  Serial.println("Sistema de controle de 4 compressores iniciado.");
}

// ================================================================= //
//                       CALLBACK DO MQTT                            //
// ================================================================= //
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // REQUISITO DE TEMPO REAL: Callback mínima, apenas passa o dado para a fila.
  payload[length] = '\0'; 
  float pressure = atof((char*)payload);
  
  // Log de diagnóstico
  Serial.printf("[MQTT-Callback] Tópico: %s, Pressão: %.2f bar\n", topic, pressure);
  
  // Envia para a fila (não bloqueante para não travar a callback)
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(pressureDataQueue, &pressure, &xHigherPriorityTaskWoken);
  
  if (xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

// ================================================================= //
//               FUNÇÃO DE RECONEXÃO MQTT ROBUSTA                   //
// ================================================================= //
bool reconnectMQTT(int &reconnectAttempts) {
  Serial.print("Tentando conexão MQTT...");
  
  // Backoff exponencial para reconexão
  int reconnectDelay = min(INITIAL_RECONNECT_DELAY * (1 << min(reconnectAttempts, 10)), MAX_RECONNECT_DELAY);
  
  // Verifica qualidade do sinal WiFi
  if (WiFi.RSSI() < -85) {
    Serial.printf(" Sinal WiFi fraco (RSSI: %d dBm). ", WiFi.RSSI());
  }
  
  if (client.connect("ESP32_CompressorController")) {
    Serial.println("Conectado! Assinando tópico...");
    
    if (client.subscribe(mqtt_topic)) {
      Serial.printf("Inscrito no tópico: %s\n", mqtt_topic);
      reconnectAttempts = 0; // Reset do contador
      return true;
    } else {
      Serial.println("Falha na inscrição do tópico!");
      return false;
    }
  } else {
    Serial.printf("Falha (tentativa %d). Próxima tentativa em %d ms\n", 
                  reconnectAttempts + 1, reconnectDelay);
    reconnectAttempts = min(reconnectAttempts + 1, 15); // Limita a 15 tentativas
    return false;
  }
}

// ================================================================= //
//              TAREFA DE REDE/MQTT (BAIXA PRIORIDADE)               //
// ================================================================= //
void taskMqttHandler(void *parameter) {
  Serial.println("Tarefa MQTT (Prio Baixa) iniciada.");
  int reconnectAttempts = 0;
  unsigned long lastWifiCheck = 0;
  const unsigned long WIFI_CHECK_INTERVAL = 10000; // Verifica WiFi a cada 10 segundos
  
  for (;;) {
    unsigned long currentTime = millis();
    
    // Verifica periodicamente o estado do WiFi
    if (currentTime - lastWifiCheck > WIFI_CHECK_INTERVAL) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi desconectado. Tentando reconectar...");
        WiFi.reconnect();
        
        // Aguarda reconexão WiFi com timeout
        unsigned long wifiReconnectTimeout = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - wifiReconnectTimeout) < 10000) {
          vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("WiFi reconectado!");
          reconnectAttempts = 0; // Reset das tentativas MQTT
        } else {
          Serial.println("Falha na reconexão WiFi!");
        }
      }
      lastWifiCheck = currentTime;
    }
    
    // Gestão da conexão MQTT
    if (!client.connected()) {
      if (WiFi.status() == WL_CONNECTED) {
        if (!reconnectMQTT(reconnectAttempts)) {
          // Aguarda o delay calculado antes da próxima tentativa
          int delayTime = min(INITIAL_RECONNECT_DELAY * (1 << min(reconnectAttempts, 10)), MAX_RECONNECT_DELAY);
          vTaskDelay(pdMS_TO_TICKS(delayTime));
          continue; // Volta ao início do loop para verificar WiFi novamente
        }
      } else {
        Serial.println("WiFi não conectado. Aguardando...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
    }
    
    // REQUISITO DE TEMPO REAL: Processa mensagens MQTT
    client.loop();
    vTaskDelay(pdMS_TO_TICKS(50)); // Delay otimizado para balance entre responsividade e CPU
  }
}

// ================================================================= //
//               TAREFA DE CONTROLE (ALTA PRIORIDADE)                //
// ================================================================= //
void taskControlLogic(void *parameter) {
  Serial.println("Tarefa de Controle (Prio Alta) iniciada.");
  float receivedPressure;

  for (;;) {
    // REQUISITO DE TEMPO REAL: Bloqueia aguardando dados, sem consumir CPU.
    if (xQueueReceive(pressureDataQueue, &receivedPressure, portMAX_DELAY) == pdTRUE) {
      Serial.printf("[Control Task] Pressão: %.2f bar\n", receivedPressure);

      // --- LÓGICA DE CONTROLE PARA 4 COMPRESSORES ---
      
      // CONDIÇÃO PARA LIGAR UM COMPRESSOR
      if (receivedPressure <= PRESSAO_LIGA_COMPRESSOR) {
        bool compressorLigado = false;
        // Tenta ligar o próximo compressor na sequência de rodízio
        for (int i = 0; i < NUM_COMPRESSORS; i++) {
          int compressorIndex = (nextCompressorToTurnOn + i) % NUM_COMPRESSORS;
          if (digitalRead(RELAY_PINS[compressorIndex]) == LOW) {
            Serial.printf("  -> AÇÃO: Pressão baixa. Ligando Compressor %d.\n", compressorIndex + 1);
            digitalWrite(RELAY_PINS[compressorIndex], HIGH);
            compressorOnTime[compressorIndex] = millis(); // Registra o tempo que foi ligado
            nextCompressorToTurnOn = (compressorIndex + 1) % NUM_COMPRESSORS; // Atualiza o próximo da fila
            compressorLigado = true;
            break; // Liga apenas um por ciclo de decisão
          }
        }
        if (!compressorLigado) {
          Serial.println("  -> INFO: Pressão baixa, mas todos os compressores já estão ligados.");
        }
      } 
      // CONDIÇÃO PARA DESLIGAR UM COMPRESSOR
      else if (receivedPressure >= PRESSAO_DESLIGA_COMPRESSOR) {
        int compressorToTurnOff = -1;
        unsigned long longestOnTime = 0;

        // Encontra o compressor que está ligado há mais tempo
        for (int i = 0; i < NUM_COMPRESSORS; i++) {
          if (digitalRead(RELAY_PINS[i]) == HIGH) {
            unsigned long onDuration = millis() - compressorOnTime[i];
            if (onDuration > longestOnTime) {
              longestOnTime = onDuration;
              compressorToTurnOff = i;
            }
          }
        }

        if (compressorToTurnOff != -1) {
          Serial.printf("  -> AÇÃO: Pressão alta. Desligando Compressor %d (ligado há mais tempo).\n", compressorToTurnOff + 1);
          digitalWrite(RELAY_PINS[compressorToTurnOff], LOW);
          compressorOnTime[compressorToTurnOff] = 0; // Reseta o tempo
        } else {
          Serial.println("  -> INFO: Pressão alta, mas todos os compressores já estão desligados.");
        }
      }
      // FAIXA DE HISTERESE: Nenhuma ação é tomada
      else {
        Serial.println("  -> INFO: Pressão na faixa de histerese. Mantendo estado atual.");
      }
    }
  }
}

// ================================================================= //
//                        LOOP PRINCIPAL (VAZIO)                     //
// ================================================================= //
void loop() {
  // O FreeRTOS gerencia a execução de todas as tarefas de forma preemptiva.
}