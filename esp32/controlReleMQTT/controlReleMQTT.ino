//Incluisão das bibliotecas do FreeRTOS
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
const char* mqtt_topic = "wokwi/pressao/pressureMQTT"; 

// Sensor
const int adcPin = 34; // Pino ADC para o sensor de pressão

// --- CONFIGURAÇÕES DE TEMPO REAL ---
#define SENSOR_READ_PERIOD_MS 1000 // Período da tarefa periódica de leitura (1 segundo)
#define MQTT_TASK_DELAY_MS 20      // A tarefa de rede executa a cada 20ms

// REQUISITO DE TEMPO REAL: Definir prioridades para as tarefas
#define SENSOR_TASK_PRIORITY 2 // Prioridade mais alta para a tarefa crítica
#define MQTT_TASK_PRIORITY   1 // Prioridade mais baixa para a tarefa não-crítica

// --- OBJETOS GLOBAIS ---
WiFiClient espClient;
PubSubClient client(espClient);

// REQUISITO DE TEMPO REAL: Criar um handle para a fila que comunicará as tarefas
QueueHandle_t pressureQueue;

// --- DECLARAÇÃO DAS TAREFAS (FreeRTOS) ---
void taskSensorReader(void *parameter);
void taskMqttHandler(void *parameter);

// ================================================================= //
//                            SETUP                                  //
// ================================================================= //
void setup() {
  Serial.begin(115200);
  delay(1000); // Pequeno delay para estabilizar o serial monitor

  // Conecta ao Wi-Fi
  Serial.print("Conectando ao Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado ao Wi-Fi!");

  // Configura o servidor MQTT
  client.setServer(mqtt_server, mqtt_port);

  // REQUISITO DE TEMPO REAL: Cria a fila para 5 itens do tipo float
  pressureQueue = xQueueCreate(5, sizeof(float));

  if (pressureQueue == NULL) {
    Serial.println("Erro ao criar a Fila!");
  }

  // REQUISITO DE TEMPO REAL: Cria as tarefas e as fixa nos núcleos do ESP32
  // Tarefa de alta prioridade no núcleo 1
  xTaskCreatePinnedToCore(
      taskSensorReader,     // Função da tarefa
      "Sensor Reader Task", // Nome para depuração
      2048,                 // Tamanho da pilha (stack size)
      NULL,                 // Parâmetros da tarefa
      SENSOR_TASK_PRIORITY, // Prioridade
      NULL,                 // Handle da tarefa
      1);                   // Núcleo (Core) 1

  // Tarefa de baixa prioridade no núcleo 0
  xTaskCreatePinnedToCore(
      taskMqttHandler,      // Função da tarefa
      "MQTT Handler Task",  // Nome para depuração
      4096,                 // Pilha maior por causa das operações de rede
      NULL,                 // Parâmetros da tarefa
      MQTT_TASK_PRIORITY,   // Prioridade
      NULL,                 // Handle da tarefa
      0);                   // Núcleo (Core) 0

  Serial.println("Sistema de tempo real iniciado. Tarefas criadas.");
}

// ================================================================= //
//                    TAREFA DE LEITURA (ALTA PRIORIDADE)            //
// ================================================================= //
void taskSensorReader(void *parameter) {
  Serial.println("Tarefa de Leitura (Prio Alta) iniciada.");
  for (;;) { // Laço infinito da tarefa
    // --- Aquisição de Dados ---
    int adcValue = analogRead(adcPin);
    float pressure = map(adcValue, 0, 4095, 0, 1000) / 100.0;
    
    // REQUISITO DE TEMPO REAL: Envia o dado para a fila de forma segura
    // A tarefa de baixa prioridade será notificada para pegar este dado.
    xQueueSend(pressureQueue, &pressure, portMAX_DELAY);
    
    // REQUISITO DE TEMPO REAL: Aguarda de forma NÃO-BLOQUEANTE pelo próximo ciclo.
    // O processador fica livre para executar a tarefa MQTT durante esta espera.
    vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS));
  }
}

//                 TAREFA DE COMUNICAÇÃO (BAIXA PRIORIDADE)          //
void taskMqttHandler(void *parameter) {
  Serial.println("Tarefa MQTT (Prio Baixa) iniciada.");
  float receivedPressure;

  for (;;) { // Laço infinito da tarefa
    // REQUISITO DE TEMPO REAL: A manutenção da conexão é feita de forma não-bloqueante
    if (!client.connected()) {
      Serial.print("MQTT desconectado. Tentando reconectar...");
      if (client.connect("ESP32_RealTimeClient")) {
        Serial.println("Conectado!");
      } else {
        Serial.print("Falha na reconexão, rc=");
        Serial.print(client.state());
        // Espera um pouco antes de tentar de novo, mas não bloqueia a outra tarefa
        vTaskDelay(pdMS_TO_TICKS(2000)); 
        continue; // Pula o resto do laço e tenta de novo
      }
    }
    client.loop(); // Mantém a conexão MQTT viva, deve ser chamado frequentemente.

    // REQUISITO DE TEMPO REAL: Verifica se há um novo dado na fila sem bloquear
    // O timeout de 0 faz com que a função retorne imediatamente se a fila estiver vazia.
    if (xQueueReceive(pressureQueue, &receivedPressure, 0) == pdTRUE) {
      Serial.print("[MQTT Task] Novo dado recebido: ");
      Serial.print(receivedPressure);
      Serial.println(" bar. Publicando...");
      
      // Formata e publica a mensagem
      char payload[10];
      snprintf(payload, sizeof(payload), "%.2f", receivedPressure);
      client.publish(mqtt_topic, payload);
    }
    
    // REQUISITO DE TEMPO REAL: Pequeno delay para ceder o processador
    // e garantir que outras tarefas de baixa prioridade (ou o idle) possam rodar.
    vTaskDelay(pdMS_TO_TICKS(MQTT_TASK_DELAY_MS));
  }
}

void loop() {
  // O loop principal fica vazio. O FreeRTOS agora gerencia a execução das tarefas.
}