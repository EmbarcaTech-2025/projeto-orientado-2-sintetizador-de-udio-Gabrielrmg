/**
 * Author: Gabriel Martins Ribeiro
 * Date: June 2025
 * Description:
 *   BitDogLab Audio-Synth – captures 10 s of audio at 16 kHz from a microphone,
 *   buffers it in RAM, and plays it back via PWM to a buzzer. At the same time
 *   it renders a 12-bar VU-meter in real-time on a 128×64 SSD1306 OLED display.
 *
 * Hardware Components:
 *   • BitDogLab board (RP2040 MCU)
 *   • Electret microphone → ADC2 (GPIO28)
 *   • Buzzer → GPIO10 (PWM)
 *   • Push-button A (GP5) → record trigger
 *   • Push-button B (GP6) → playback trigger
 *   • Red LED → GPIO13 (record indicator)
 *   • Green LED → GPIO11 (playback indicator)
 *   • OLED display SSD1306 → I2C1 (GPIO14 SDA, GPIO15 SCL)
 *
 * Functionality:
 *   • On button A press: start recording 10 s of 12-bit samples at 16 kHz,
 *     downsample to 8 bit, update VU-meter live on the OLED, and light the red LED.
 *   • Store samples in a circular RAM buffer (160 kB).
 *   • On button B press: play back the recorded samples via PWM at 16 kHz,
 *     update VU-meter live on the OLED, and light the green LED.
 *   • Use repeating timers for non-blocking sample I/O and display refresh.
 *   • Provide smooth, frame-buffered updates to avoid OLED flicker.
 */


#include <stdio.h>                     // Inclui a biblioteca padrão de entrada e saída
#include <string.h>                   // Inclui a biblioteca para manipulação de strings
#include "pico/stdlib.h"              // Inclui a biblioteca padrão do Raspberry Pi Pico
#include "hardware/adc.h"             // Inclui a biblioteca para controle do ADC (Conversor Analógico-Digital)
#include "hardware/pwm.h"             // Inclui a biblioteca para controle de PWM (Modulação por Largura de Pulso)
#include "hardware/i2c.h"             // Inclui a biblioteca para comunicação I2C
#include "pico/time.h"                // Inclui a biblioteca para manipulação de tempo
#include "ssd1306.h"                  // Inclui a biblioteca para controle do display OLED SSD1306
#include "font.h"                     // Inclui a biblioteca para manipulação de fontes

/*───────────────── ÁUDIO ─────────────────────────────*/
#define SAMPLE_RATE_HZ   16000       // Define a taxa de amostragem em Hertz
#define DURATION_SEC         10       // Define a duração da gravação em segundos
#define NUM_SAMPLES   (SAMPLE_RATE_HZ * DURATION_SEC) // Calcula o número total de amostras
#define TICK_US              62       /* 1/16 kHz */ // Define o intervalo de tempo em microssegundos

/*───────────────── PINOS BITDOGLAB ───────────────────*/
#define BTN_REC_PIN            5     /* GP5  → REC  */ // Define o pino do botão de gravação
#define BTN_PLAY_PIN           6     /* GP6  → PLAY */ // Define o pino do botão de reprodução
#define LED_R_PIN             13      // Define o pino do LED vermelho
#define LED_G_PIN             11      // Define o pino do LED verde
#define BUZZER_PIN            10      // Define o pino do buzzer
#define MIC_GPIO              28      // Define o pino do microfone
#define MIC_CH                 2      // Define o canal do microfone

/*───────────────── OLED SSD1306 (I²C1) ───────────────*/
#define OLED_W               128      // Define a largura do display OLED
#define OLED_H                64      // Define a altura do display OLED
#define I2C_PORT            i2c1      // Define a porta I2C a ser utilizada
#define I2C_SDA_PIN           14      // Define o pino SDA para I2C
#define I2C_SCL_PIN           15      // Define o pino SCL para I2C
#define OLED_ADDR           0x3C      // Define o endereço do display OLED

/*───────────────── VU-METER ──────────────────────────*/
#define BAR_COUNT             12      // Define o número de barras do VU-meter
#define BAR_W                  5      // Define a largura de cada barra
#define BAR_GAP                5      // Define o espaço entre as barras
#define BAR_AREA_W   ((BAR_W+BAR_GAP)*BAR_COUNT - BAR_GAP)   /* 115 px */ // Calcula a largura total da área das barras
#define BAR_X0        ((OLED_W - BAR_AREA_W)/2)              /* 6 px  */ // Calcula a posição X inicial das barras
#define BAR_H                 64      // Define a altura das barras
#define BAR_TOP        (OLED_H - BAR_H)   /* 0 */ // Define a posição Y superior das barras

static ssd1306_t oled;               // Declara uma estrutura para o display OLED
/* framebuffer principal já está dentro de oled.buffer   */
static uint8_t backbuf[OLED_W*OLED_H/8];    /* 1024 bytes - cópia */ // Declara um buffer de cópia para o framebuffer

/*───────────────── ESTADO GERAL ──────────────────────*/
typedef enum {IDLE=0, REC, PLAY} state_t; // Define os estados possíveis do sistema
static volatile state_t state = IDLE; // Declara a variável de estado como volátil

/* áudio 8 bit (160 kB) */
static uint8_t buffer[NUM_SAMPLES]; // Declara um buffer para armazenar as amostras de áudio
static volatile uint32_t wr_i=0, rd_i=0; // Índices de escrita e leitura, ambos voláteis
static volatile bool rec_done=false, play_done=false; // Flags para indicar se a gravação ou reprodução estão concluídas
static volatile bool frame_ready=false; // Flag para indicar se o frame está pronto para ser exibido

/* debounce */
static volatile uint32_t last_irq=0; // Armazena o último tempo de interrupção
#define DEBOUNCE_US 200000 // Define o tempo de debounce em microssegundos

/*───────────────── DESENHO ───────────────────────────*/
static inline void clear_bar(uint8_t idx) // Função para limpar uma barra do VU-meter
{
    uint8_t x0 = BAR_X0 + idx*(BAR_W+BAR_GAP); // Calcula a posição X da barra a ser limpa
    for (uint8_t page = BAR_TOP>>3; page < 8; ++page) // Itera pelas páginas do display
        memset(&oled.buffer[page*oled.width + x0], 0, BAR_W); // Limpa a barra no buffer
}

static inline void draw_bar(uint8_t idx, uint8_t h) // Função para desenhar uma barra do VU-meter
{
    uint8_t x0 = BAR_X0 + idx*(BAR_W+BAR_GAP); // Calcula a posição X da barra a ser desenhada
    for (uint8_t y=0; y<=h; ++y) // Itera pela altura da barra
        for (uint8_t x=0; x<BAR_W; ++x) // Itera pela largura da barra
            ssd1306_draw_pixel(&oled, x0+x, OLED_H-1 - y); // Desenha o pixel no buffer do OLED
}

/*───────────────── CALLBACKS ─────────────────────────*/
static bool adc_cb(repeating_timer_t*) // Callback para o ADC
{
    if (wr_i >= NUM_SAMPLES){ rec_done=true; return false; } // Verifica se a gravação está completa
    uint8_t v = adc_read() >> 4;           /* 0-255 */ // Lê o valor do ADC e o ajusta para 0-255
    buffer[wr_i++] = v; // Armazena o valor no buffer de áudio
    uint8_t h   = (BAR_H-1)*v/255u; // Calcula a altura da barra com base no valor lido
    uint8_t idx = wr_i % BAR_COUNT; // Calcula o índice da barra a ser atualizada
    clear_bar(idx); // Limpa a barra anterior
    draw_bar (idx,h); // Desenha a nova barra
    if (idx == BAR_COUNT-1) frame_ready = true; // Marca o frame como pronto se todas as barras foram atualizadas
    return true; // Retorna verdadeiro para continuar o timer
}

static bool pwm_cb(repeating_timer_t*) // Callback para o PWM
{
    if (rd_i >= NUM_SAMPLES){ play_done=true; return false; } // Verifica se a reprodução está completa
    uint8_t v = buffer[rd_i++]; // Lê o valor do buffer de áudio
    uint8_t h = (BAR_H-1)*v/255u; // Calcula a altura da barra com base no valor lido
    uint8_t idx = rd_i % BAR_COUNT; // Calcula o índice da barra a ser atualizada
    uint8_t duty=v; if(duty<30)duty=30; if(duty>220)duty=220; // Ajusta o valor do duty cycle para o PWM
    pwm_set_gpio_level(BUZZER_PIN,duty); // Define o nível do PWM no pino do buzzer
    clear_bar(idx); // Limpa a barra anterior
    draw_bar (idx,h); // Desenha a nova barra
    if (idx == BAR_COUNT-1) frame_ready = true; // Marca o frame como pronto se todas as barras foram atualizadas
    return true; // Retorna verdadeiro para continuar o timer
}

/*───────────────── BOTÕES ISR ────────────────────────*/
static void btn_isr(uint gpio, uint32_t) // Função de interrupção para os botões
{
    uint32_t now=time_us_32(); // Obtém o tempo atual em microssegundos
    if(now-last_irq<DEBOUNCE_US) return; // Verifica se o tempo de debounce foi respeitado
    last_irq=now; // Atualiza o último tempo de interrupção
    state = (gpio==BTN_REC_PIN)?REC:PLAY; // Atualiza o estado com base no botão pressionado
}

/*───────────────── OLED INIT ─────────────────────────*/
static void init_oled(void) // Função para inicializar o display OLED
{
    i2c_init(I2C_PORT,400*1000); // Inicializa a comunicação I2C com a taxa de 400 kHz
    gpio_set_function(I2C_SDA_PIN,GPIO_FUNC_I2C); // Configura o pino SDA para função I2C
    gpio_set_function(I2C_SCL_PIN,GPIO_FUNC_I2C); // Configura o pino SCL para função I2C
    gpio_pull_up(I2C_SDA_PIN); gpio_pull_up(I2C_SCL_PIN); // Ativa os resistores de pull-up nos pinos I2C
    ssd1306_init(&oled, OLED_W, OLED_H, OLED_ADDR, I2C_PORT); // Inicializa o display OLED
    ssd1306_clear(&oled); // Limpa o display OLED
    ssd1306_draw_string(&oled, 8,0,1, "AUDIO SYNTH !"); // Desenha a string "AUDIO SYNTH !" no display
    ssd1306_draw_string(&oled, 0, 10, 1, "PRESS A TO REC"); // Desenha a instrução para gravar no display
    ssd1306_show(&oled); // Atualiza o display para mostrar as strings desenhadas
}

/*───────────────── FLUSH NÃO-BLOQUEANTE ─────────────*/
static inline void flush_if_ready(void) // Função para atualizar o display se o frame estiver pronto
{
    if(!frame_ready) return; // Retorna se o frame não estiver pronto
    frame_ready = false; // Marca o frame como não pronto
    memcpy(backbuf, oled.buffer, sizeof(backbuf)); // Copia o buffer atual para o backbuffer
    uint8_t *orig = oled.buffer; // Armazena o ponteiro original do buffer
    oled.buffer   = backbuf; // Define o buffer do OLED como o backbuffer
    ssd1306_show(&oled);          /* envia back-buffer */ // Atualiza o display com o backbuffer
    oled.buffer   = orig;         /* volta ao principal*/ // Restaura o buffer original
}

/*───────────────── FUNÇÕES HIGH-LEVEL ───────────────*/
static void do_record(void) // Função para iniciar a gravação
{
    printf(" Gravando 10 s…\n"); // Exibe mensagem de gravação
    gpio_put(LED_R_PIN,1); // Acende o LED vermelho
    ssd1306_clear(&oled); // Limpa o display OLED
    adc_init(); adc_gpio_init(MIC_GPIO); adc_select_input(MIC_CH); // Inicializa o ADC e configura o microfone
    wr_i=0; rec_done=false; frame_ready=true; // Reseta os índices e flags
    repeating_timer_t t; // Declara um timer repetitivo
    add_repeating_timer_us(-TICK_US, adc_cb, NULL, &t); // Adiciona o timer para chamar o callback do ADC
    while(!rec_done){ flush_if_ready(); } // Enquanto a gravação não estiver concluída, atualiza o display
    cancel_repeating_timer(&t); // Cancela o timer após a gravação
    gpio_put(LED_R_PIN,0); // Apaga o LED vermelho
    printf(" Gravacao pronta\n"); // Exibe mensagem de gravação concluída
}

static void do_play(void) // Função para iniciar a reprodução
{
    printf(" Reproduzindo…\n"); // Exibe mensagem de reprodução
    gpio_put(LED_G_PIN,1); // Acende o LED verde
    gpio_set_function(BUZZER_PIN,GPIO_FUNC_PWM); // Configura o pino do buzzer para função PWM
    uint slice=pwm_gpio_to_slice_num(BUZZER_PIN); // Obtém o número do slice PWM associado ao pino do buzzer
    pwm_config cfg=pwm_get_default_config(); // Obtém a configuração padrão do PWM
    pwm_config_set_clkdiv(&cfg,1.0f); // Define o divisor de clock do PWM
    pwm_config_set_wrap (&cfg,255); // Define o valor máximo do PWM
    pwm_init(slice,&cfg,true); // Inicializa o PWM com a configuração definida
    rd_i=0; play_done=false; frame_ready=true; // Reseta os índices e flags
    repeating_timer_t t; // Declara um timer repetitivo
    add_repeating_timer_us(-TICK_US, pwm_cb, NULL, &t); // Adiciona o timer para chamar o callback do PWM
    while(!play_done){ flush_if_ready(); } // Enquanto a reprodução não estiver concluída, atualiza o display
    cancel_repeating_timer(&t); // Cancela o timer após a reprodução
    gpio_put(LED_G_PIN,0); // Apaga o LED verde
    printf(" Reproducao ok\n"); // Exibe mensagem de reprodução concluída
}

/*───────────────── MAIN ─────────────────────────────*/
int main(void) // Função principal
{
    stdio_init_all(); // Inicializa a entrada e saída padrão
    while(!stdio_usb_connected()) sleep_ms(100); // Aguarda a conexão USB
    init_oled(); // Inicializa o display OLED
    /* botões */
    gpio_init(BTN_REC_PIN);  gpio_set_dir(BTN_REC_PIN,GPIO_IN); // Inicializa o pino do botão de gravação como entrada
    gpio_pull_up(BTN_REC_PIN); // Ativa o resistor de pull-up no botão de gravação
    gpio_set_irq_enabled_with_callback(BTN_REC_PIN, // Configura a interrupção para o botão de gravação
        GPIO_IRQ_EDGE_FALL,true,&btn_isr);
    gpio_init(BTN_PLAY_PIN); gpio_set_dir(BTN_PLAY_PIN,GPIO_IN); // Inicializa o pino do botão de reprodução como entrada
    gpio_pull_up(BTN_PLAY_PIN); // Ativa o resistor de pull-up no botão de reprodução
    gpio_set_irq_enabled(BTN_PLAY_PIN, // Configura a interrupção para o botão de reprodução
        GPIO_IRQ_EDGE_FALL,true);
    /* LEDs */
    gpio_init(LED_R_PIN); gpio_set_dir(LED_R_PIN,GPIO_OUT); // Inicializa o pino do LED vermelho como saída
    gpio_init(LED_G_PIN); gpio_set_dir(LED_G_PIN,GPIO_OUT); // Inicializa o pino do LED verde como saída
    printf("GP5 = REC | GP6 = PLAY (12×64 barras)\n"); // Exibe informações sobre os pinos dos botões
    while(true){ // Loop principal
        flush_if_ready();               /* mantém tela suave */ // Atualiza o display se o frame estiver pronto
        switch(state){ // Verifica o estado atual
        case REC:  do_record(); state=IDLE; break; // Se estiver gravando, chama a função de gravação e volta ao estado IDLE
        case PLAY: do_play();   state=IDLE; break; // Se estiver reproduzindo, chama a função de reprodução e volta ao estado IDLE
        default:   tight_loop_contents(); // Caso contrário, mantém o loop apertado
        }
    }
}
