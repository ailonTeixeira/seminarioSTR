import tkinter as tk
from tkinter import font 
import requests
import json
from datetime import datetime
from PIL import Image, ImageTk
import io
import paho.mqtt.client as mqtt
import threading
import queue

# --- CONFIGURAÇÕES ---
API_KEY = ""
COORDINATES = (-3.110193661819573, -59.95485251396774)
PRESSURE_LIGA = 7.0   # Limite inferior para alerta laranja
PRESSURE_DESLIGA = 9.0   # Limite superior para alerta vermelho

# Cores do Alerta
COLOR_NORMAL = "#2c3e50" # Azul escuro
COLOR_ALERTA_BAIXA = "#f39c12" # Laranja
COLOR_ALERTA_ALTA = "#e74c3c" # Vermelho

# MQTT
MQTT_BROKER = ""
MQTT_TOPIC = "" 

# REQUISITO DE TEMPO REAL: Fila para comunicação segura entre threads
gui_queue = queue.Queue()

# =============================================================================
# TAREFA 1: MQTT CLIENT (Executa em sua própria thread)
# Prioridade: Média (responsável por receber dados críticos)
# =============================================================================
def on_connect(client, userdata, flags, rc):
    print(f"Conectado ao broker MQTT com código {rc}")
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    try:
        pressure_value = float(msg.payload.decode())
        # REQUISITO DE TEMPO REAL: Coloca o dado na fila em vez de atualizar a GUI diretamente
        gui_queue.put(("PRESSURE_UPDATE", pressure_value))
    except (ValueError, UnicodeDecodeError):
        print("Dados de pressão inválidos recebidos via MQTT")

def mqtt_client_thread():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    try:
        client.connect(MQTT_BROKER, 1883, 60)
        client.loop_forever()
    except Exception as e:
        print(f"Erro crítico no cliente MQTT: {e}")
        gui_queue.put(("MQTT_ERROR", str(e)))

# =============================================================================
# TAREFA 2: WEATHER FETCHER (Executa em sua própria thread)
# Prioridade: Baixa (operação lenta, não-crítica e imprevisível)
# =============================================================================
def weather_fetcher_thread():
    while True:
        try:
            url = f"http://api.openweathermap.org/data/2.5/weather?lat={COORDINATES[0]}&lon={COORDINATES[1]}&appid={API_KEY}&units=metric&lang=pt"
            response = requests.get(url, timeout=10)
            data = response.json()

            if response.status_code == 200:
                # REQUISITO DE TEMPO REAL: Envia os dados processados para a fila
                gui_queue.put(("WEATHER_UPDATE", data))
            else:
                raise Exception(f"Erro {data.get('cod')}: {data.get('message', 'Sem dados')}")
        except Exception as e:
            print(f"Erro ao buscar clima: {e}")
            gui_queue.put(("WEATHER_ERROR", "Erro de API"))
        
        # Espera um longo período antes da próxima requisição
        time.sleep(300) # Atualiza a cada 5 minutos

# =============================================================================
# TAREFA 3: INTERFACE GRÁFICA (Thread Principal)
# Prioridade: Alta (deve permanecer sempre responsiva)
# =============================================================================
def update_gui_loop():
    """Loop principal que verifica a fila e atualiza a interface."""
    try:
        # Processa todas as mensagens na fila sem bloquear
        while not gui_queue.empty():
            message_type, data = gui_queue.get_nowait()
            
            if message_type == "PRESSURE_UPDATE":
                update_pressure_display(data)
            elif message_type == "WEATHER_UPDATE":
                update_weather_display(data)
            elif message_type == "MQTT_ERROR":
                pressure_label.config(text=f"MQTT Err", fg='white')
                set_background_color(COLOR_ALERTA_ALTA)

    except queue.Empty:
        pass # Normal, a fila estava vazia

    # Atualiza o relógio
    now = datetime.now().strftime("%H:%M:%S")
    time_label.config(text=now)

    # Agenda a próxima verificação
    root.after(100, update_gui_loop) # Verifica a fila a cada 100ms

def set_background_color(color):
    """Muda a cor de fundo de todos os widgets principais."""
    root.configure(bg=color)
    for widget in root.winfo_children():
        widget.configure(bg=color)
        if isinstance(widget, tk.Frame):
            for sub_widget in widget.winfo_children():
                sub_widget.configure(bg=color)


def update_pressure_display(pressure):
    pressure_label.config(text=f"{pressure:.1f} bar")
    
    if pressure > PRESSURE_DESLIGA:
        set_background_color(COLOR_ALERTA_ALTA) # Vermelho
    elif pressure < PRESSURE_LIGA:
        set_background_color(COLOR_ALERTA_BAIXA) # Laranja
    else:
        set_background_color(COLOR_NORMAL) # Azul

def update_weather_display(data):
    try:
        temp = data["main"]["temp"]
        desc = data["weather"][0]["description"].capitalize()
        icon_code = data["weather"][0]["icon"]

        temp_label.config(text=f"{temp:.1f}°C")
        desc_label.config(text=f"{desc} | Distrito Industrial I")

        icon_url = f"http://openweathermap.org/img/wn/{icon_code}@2x.png"
        image_data = requests.get(icon_url, stream=True, timeout=5).raw
        img = Image.open(image_data).resize((50, 50), Image.LANCZOS)
        weather_icon = ImageTk.PhotoImage(img)
        icon_label.config(image=weather_icon)
        icon_label.image = weather_icon
    except Exception as e:
        print(f"Erro ao processar imagem do clima: {e}")
        desc_label.config(text="Erro de Imagem")

# --- Interface Gráfica (Criação dos Widgets) ---
root = tk.Tk()
root.title("SISTEMAS DE TEMPO REAL - MONITORAMENTO MQTT")
root.configure(bg=COLOR_NORMAL)

# Fontes
titulo_font = font.Font(family='Arial', size=40, weight='bold')
temp_font = font.Font(family='Arial', size=50)
desc_font = font.Font(family='Arial', size=30)
time_font = font.Font(family='Arial', size=200, weight='bold')
pressure_font = font.Font(family='Arial', size=100, weight='bold')
pressure_desc_font = font.Font(family='Arial', size=50)

# Widgets
titulo_label = tk.Label(root, text="MONITORAMENTO DE PRESSÃO", font=titulo_font, fg='white')
titulo_label.pack(pady=20)
temp_label = tk.Label(root, font=temp_font, fg='white')
temp_label.pack()
icon_label = tk.Label(root)
icon_label.pack()
desc_label = tk.Label(root, font=desc_font, fg='white')
desc_label.pack(pady=10)
time_label = tk.Label(root, font=time_font, fg='white')
time_label.pack(expand=True)

pressure_frame = tk.Frame(root)
pressure_frame.pack(pady=30)
pressure_desc_label = tk.Label(pressure_frame, text="Pressão na Rede:", font=pressure_desc_font, fg='white')
pressure_desc_label.pack(side=tk.LEFT, padx=20)
pressure_label = tk.Label(pressure_frame, font=pressure_font, fg='white')
pressure_label.pack(side=tk.LEFT)

set_background_color(COLOR_NORMAL) # Garante cor inicial correta

# --- INICIALIZAÇÃO DAS TAREFAS DE FUNDO E LOOP PRINCIPAL ---
if __name__ == "__main__":
    # Inicia as threads de baixa prioridade
    threading.Thread(target=mqtt_client_thread, daemon=True).start()
    threading.Thread(target=weather_fetcher_thread, daemon=True).start()

    # Inicia o loop de atualização da GUI
    update_gui_loop()
    
    # Configurações de tela cheia
    root.overrideredirect(True)
    root.geometry("{0}x{1}+0+0".format(root.winfo_screenwidth(), root.winfo_screenheight()))
    root.focus_set()
    root.bind("<Escape>", lambda e: root.destroy())

    # Inicia o loop principal da interface
    root.mainloop()
