# Seminário de Sistemas de Tempo Real

Repositório público para adicionar os documentos do Seminário de Sistemas de Tempo Real.

## Estrutura do Projeto

O repositório está dividido em diferentes diretórios, cada um com propósito específico para o estudo e demonstração de sistemas de tempo real:

- **appWeb/**
  - [`app.py`](https://github.com/ailonTeixeira/seminarioSTR/blob/main/appWeb/app.py): Aplicação web (Python), utilizada para visualização e controle do sistema.
  - [`templates/`](https://github.com/ailonTeixeira/seminarioSTR/tree/main/appWeb/templates): Arquivos de template para front-end da aplicação web.

- **esp32/**
  - [`compressorAutomation/`](https://github.com/ailonTeixeira/seminarioSTR/tree/main/esp32/compressorAutomation): código relacionado à automação de compressor usando ESP32.
  - [`controlReleMQTT/`](https://github.com/ailonTeixeira/seminarioSTR/tree/main/esp32/controlReleMQTT): Controle dos compressores via MQTT utilizando ESP32, indicado para aplicações em tempo real.
  - [`pressureMQTT/`](https://github.com/ailonTeixeira/seminarioSTR/tree/main/esp32/pressureMQTT): Coleta e transmisão dos dados do sensor de pressão via MQTT com ESP32.

- **simulation/**
  - [`autoSimulation.py`](https://github.com/ailonTeixeira/seminarioSTR/blob/main/simulation/autoSimulation.py): Script Python para simulação automática de cenários vinculados ao seminário.
  - [`linkWokiSimulation.md`](https://github.com/ailonTeixeira/seminarioSTR/blob/main/simulation/linkWokiSimulation.md): Documentação sobre a integração/simulação com a plataforma Wokwi.
  - [`wokwiSimulation.png`](https://github.com/ailonTeixeira/seminarioSTR/blob/main/simulation/wokwiSimulation.png): Imagem ilustrativa da simulação.

- **supervision/**
  - [`appViewer.py`](https://github.com/ailonTeixeira/seminarioSTR/blob/main/supervision/appViewer.py): Código para visualização/supervisão das rotinas e processos do sistema.

## Linguagens Utilizadas

- **C++** – Automação embarcada, controle via ESP32 (51.3%)
- **Python** – Scripts de simulação, aplicação web e supervisão (32.4%)
- **HTML** – Interface web do projeto (16.3%)

## Como Utilizar

1. Clone este repositório:  
   ```sh
   git clone https://github.com/ailonTeixeira/seminarioSTR.git
   ```
2. Para aplicações web/supervisão, consulte os arquivos `app.py` e `appViewer.py` 

## Contribuição

Contribuições são bem-vindas!  
Abra issues ou pull requests para sugestões, correções ou novos exemplos e documentos.


---

Explore os diretórios para encontrar exemplos práticos, simulações e sistemas reais voltados ao estudo de tempo real!# seminarioSTR
Reposiório público para adicionar os documentos do Seminário de Sistemas de Tempo Real
