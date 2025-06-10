
# Projetos de Sistemas Embarcados - EmbarcaTech 2025

Autor: **Gabriel Martins Ribeiro**

Curso: Residência Tecnológica em Sistemas Embarcados

Instituição: EmbarcaTech - HBr

Brasília, Junho de 2025

---

# 🎶 Audio Synth

BitDogLab Audio-Synth grava **10 s** de áudio a **16 kHz**, armazena em RAM e
reproduz em um buzzer via PWM. Durante ambos os modos, exibe um **VU-meter  
12 × 64 px** em tempo real no display OLED SSD1306 (128 × 64).

---

## 📷 Demonstração

| Gravação (LED vermelho)         | Reprodução (LED verde)        |
| :-----------------------------: | :---------------------------: |
| ![Rec](img/REC_vermelho.jpg)            | ![Play](img/PLAY_verde.jpg)        |



---

## ▶ Vídeo de Demonstração

Assista ao protótipo funcionando no YouTube:

[![Veja o demo no YouTube](https://img.youtube.com/vi/sCS0l5j58IU/0.jpg)](https://youtube.com/shorts/sCS0l5j58IU?feature=share)

Clique na imagem acima para acesso ao link, ou  se preferir um link simples:

[▶ Demo no YouTube](https://youtube.com/shorts/sCS0l5j58IU?feature=share)

---


## 🗂 Estrutura do Código

| Arquivo                       | Descrição                                  |
| ----------------------------- | ------------------------------------------ |
| **main.c**                    | FSM, splash-screen, timers ADC/PWM e VU-meter |
| **ssd1306.c / ssd1306.h**     | Driver do display OLED SSD1306             |
| **font.c / font.h**           | Fonte 5 × 8 pixels usada nas strings       |
| **CMakeLists.txt**            | Script de build para Pico SDK              |
| **pico_sdk_import.cmake**     | Importação do Pico SDK                     |

---

## ✅ Funcionalidades

- Splash “AUDIO SYNTH!” aguarda botão A  
- Grava 10 s a 16 kHz (12→8 bits)  
- Reproduz via PWM a 16 kHz (8 bits)  
- VU-meter 12 barras em gravação e reprodução  
- LED vermelho ◉ gravação / LED verde ◉ reprodução  
- Botões A (GP 5) e B (GP 6)  
- Buffer duplo e flush sem flicker no OLED

---

## 🔧 Hardware

| Componente                     | Conexão                         |
| -----------------------------  | ------------------------------  |
| Pico W (RP2040)                | —                                |
| Microfone de eletreto          | ADC2 → GPIO 28                   |
| Buzzer passivo                 | PWM → GPIO 10                    |
| Botão A / Botão B              | GPIO 5 / GPIO 6                  |
| OLED SSD1306 128 × 64          | I²C1: SDA → GPIO 14  SCL → GPIO 15 |
| LED vermelho / LED verde       | GPIO 13 / GPIO 11                |

---

## ⚙️ Como Compilar

```bash
cd projects/Audio_Synth
mkdir build && cd build
cmake ..
make -j4
# Produz: AudioSynth.uf2
```
---

## 🚀 Uso

1. Ligue o Pico com o firmware.  
2. No splash **“AUDIO SYNTH!”**, pressione **A** para gravar (LED vermelho).  
3. Após 10 s, o LED apaga. Pressione **B** para reproduzir (LED verde).  
4. Observe o VU-meter no OLED e escute o buzzer.  

---

## 💭 Reflexão Final

- **Técnicas de programação que podem ser utilizadas para melhorar a gravação e a reprodução do áudio:**  
  - **DMA + ADC FIFO:** captura as amostras de forma contínua, sem overhead de CPU.  
  - **PIO + PWM DMA:** gera o sinal de saída exatamente a 16 kHz sem jitter.  
  - **Buffers circulares (ring buffer):** garante fluxo ininterrupto de dados, evitando under-/overflows.  
  - **Interrupções e timers hardware:** mantém o timing estritamente preciso para amostragem e reprodução.  
  - **Filtragem no firmware (média móvel ou passa-baixa):** reduz ruídos de alta frequência antes de armazenar ou tocar.

- **Formas de gravar áudios mais extensos, sem prejudicar a qualidade da gravação:**  
  - **Stream para memória externa (flash ou cartão SD):** libera a RAM do Pico e permite dezenas de megabytes de armazenamento.  
  - **Compressão em tempo real (ADPCM, µ-law):** reduz o tamanho das amostras de 12 ou 16 bits para 4–8 bits sem perda perceptível.  
  - **Gravação em blocos (chunking) com flush imediato:** divide o áudio em páginas e grava cada bloco no meio externo para evitar consumo excessivo de RAM.  
  - **Driver de sistema de arquivos (FAT) + DMA:** escreve diretamente no cartão SD via DMA, mantendo performance e qualidade de amostragem.

---

## 📜 Licença
GNU GPL-3.0.

