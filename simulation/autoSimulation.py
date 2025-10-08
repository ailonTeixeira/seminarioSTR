import time
import random

# --- PARÂMETROS DE CONFIGURAÇÃO DO SISTEMA (Baseado nos Slides) ---
SENSOR_PERIODO = 5       # Tarefa periódica: lê o sensor a cada 5 segundos.
PRESSAO_MIN_OK = 7.0     # Limite inferior da faixa normal (em bar).
PRESSAO_MAX_OK = 9.0     # Limite superior da faixa normal (em bar).

# --- PARÂMETROS DA SIMULAÇÃO ---
SIMULATION_DURATION = 120 # Duração total da simulação em segundos.
TIME_STEP = 1             # Passo de tempo da simulação (1 segundo).
INITIAL_PRESSURE = 6.5    # Pressão inicial para forçar o sistema a agir.
CONSUMO_AR = 0.1          # Quanto a pressão cai por segundo (simula uso).
GANHO_COMPRESSOR = 0.4    # Quanto a pressão sobe por segundo com o compressor ligado.

# --- ESTADO INICIAL DO SISTEMA ---
current_time = 0
current_pressure = INITIAL_PRESSURE
compressor_on = False
next_sensor_read_time = 0

# --- FUNÇÕES QUE SIMULAM AS TAREFAS E A LÓGICA ---

def read_pressure_sensor():
    """Simula a tarefa periódica de leitura do sensor."""
    # Adiciona um pequeno ruído para tornar a leitura mais realista.
    noise = random.uniform(-0.05, 0.05)
    return current_pressure + noise

def control_logic(pressure, compressor_state):
    """
    Simula a lógica de controle e os alertas.
    Esta é a "tarefa esporádica" acionada pelos dados do sensor.
    """
    new_compressor_state = compressor_state
    
    # Lógica de Alerta e Controle (baseado no Slide 5)
    if pressure < PRESSAO_MIN_OK:
        print(f"    -> ALERTA BAIXA: Pressão em {pressure:.1f} bar. RISCO DE PARADA!")
        if not compressor_state:
            print("    -> AÇÃO: LIGANDO COMPRESSOR.")
            new_compressor_state = True
    
    elif pressure > PRESSAO_MAX_OK:
        print(f"    -> ALERTA ALTA: Pressão em {pressure:.1f} bar. RISCO DE SEGURANÇA!")
        if compressor_state:
            print("    -> AÇÃO: DESLIGANDO COMPRESSOR.")
            new_compressor_state = False
            
    else: # Pressão está na faixa OK
        print(f"    -> STATUS: Pressão OK ({pressure:.1f} bar).")
        # Lógica de histerese: se a pressão está OK, mantém o estado do compressor
        # para evitar que ele ligue e desligue rapidamente.
        new_compressor_state = compressor_state

    return new_compressor_state

def update_physical_plant(pressure, is_on):
    """Simula a física do sistema de ar comprimido."""
    if is_on:
        # Pressão sobe quando o compressor está ligado
        return pressure + GANHO_COMPRESSOR
    else:
        # Pressão cai devido ao consumo
        return max(0, pressure - CONSUMO_AR) # Garante que a pressão não fique negativa

# --- LAÇO PRINCIPAL DA SIMULAÇÃO ---
print("--- INICIANDO SIMULAÇÃO DO SISTEMA DE CONTROLE DE PRESSÃO ---")
print(f"Faixa de Operação Normal: [{PRESSAO_MIN_OK}, {PRESSAO_MAX_OK}] bar\n")

while current_time < SIMULATION_DURATION:
    # Imprime o estado atual (simula o dashboard)
    status = "LIGADO" if compressor_on else "DESLIGADO"
    print(f"Tempo: {current_time:3d}s | Pressão Atual: {current_pressure:.2f} bar | Compressor: {status}")
    
    # 1. TAREFA PERIÓDICA: Verifica se é hora de ler o sensor
    if current_time >= next_sensor_read_time:
        print(f"  -> TAREFA PERIÓDICA: Lendo sensor no tempo {current_time}s...")
        
        # Simula a leitura
        pressure_reading = read_pressure_sensor()
        
        # 2. LÓGICA DE CONTROLE: Executa a lógica com base na leitura
        compressor_on = control_logic(pressure_reading, compressor_on)
        
        # Agenda a próxima execução da tarefa periódica
        next_sensor_read_time += SENSOR_PERIODO
        print("-" * 60)

    # 3. ATUALIZAÇÃO DO MUNDO FÍSICO: A pressão muda a cada segundo
    current_pressure = update_physical_plant(current_pressure, compressor_on)

    # Avança o tempo da simulação
    time.sleep(0.1) # Pausa para tornar a saída legível
    current_time += TIME_STEP

print("--- FIM DA SIMULAÇÃO ---")
