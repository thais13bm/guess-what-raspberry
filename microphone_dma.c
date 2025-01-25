#include <stdio.h>
//#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
//#include "hardware/dma.h"
//#include "neopixel.c"
#include "pico/cyw43_arch.h"   //e se eu botar o path completo?

#include "lwip/tcp.h"
#include <stdint.h>

#define WAV_SAMPLE_RATE  22050
#define WAV_PWM_COUNT   (125000000 / WAV_SAMPLE_RATE)

#define SERVER_IP "192.168.1.193" // Substitua pelo IP do servidor Flask
#define SERVER_PORT 5000

// Pino e canal do microfone no ADC.
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)





// Parâmetros e macros do ADC.
#define ADC_CLOCK_DIV 2177.28

//#define ADC_CLOCK_DIV 96.f
//#define ADC_CLOCK_DIV 3000.f  //novo teste

#define CHUNK_SIZE 1024
#define AUDIO_PERIOD    3
#define SAMPLES      (WAV_SAMPLE_RATE * AUDIO_PERIOD)  //acho que nao tem necessidade, ja que ainda quero fazer isso no servidor mesmo


//#define SAMPLES 400 // Número de amostras que serão feitas do ADC.

//#define SAMPLES 80000

#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f) // Ajuste do valor do ADC para Volts.
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f/5.f) // Intervalos de volume do microfone.

// Pino e número de LEDs da matriz de LEDs.
#define LED_PIN 7
#define LED_COUNT 25




// Canal e configurações do DMA
/*uint dma_channel;
dma_channel_config dma_cfg;
*/
// Buffer de amostras do ADC.
uint16_t adc_buffer[SAMPLES];
static struct tcp_pcb *client_pcb;


//declaracao de funcoes


/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
/*void sample_mic() {
  adc_fifo_drain(); // Limpa o FIFO do ADC.
  adc_run(false); // Desliga o ADC (se estiver ligado) para configurar o DMA.

  dma_channel_configure(dma_channel, &dma_cfg,
    adc_buffer, // Escreve no buffer.
    &(adc_hw->fifo), // Lê do ADC.
    SAMPLES, // Faz SAMPLES amostras.
    true // Liga o DMA.
  );

  // Liga o ADC e espera acabar a leitura.
  adc_run(true);
  dma_channel_wait_for_finish_blocking(dma_channel);
  
  // Acabou a leitura, desliga o ADC de novo.
  adc_run(false);

  for (size_t i = 0; i < SAMPLES; i++) {
        
        int16_t audio_sample = (int16_t)((adc_buffer[i] * 32) - 32768); // Converte para escala de áudio.

        // Substitui os valores no buffer original (ou use outro buffer, se preferir).
        adc_buffer[i] = audio_sample;
    }

}*/

void sample_mic_no_dma()
{
   unsigned long SampleCount = 0;
   unsigned long long SamplePeriod;

  
   SampleCount = 0;
  
  /**************************************************************/
 /* Fill audio buffer with values read from the A/D converter. */
/**************************************************************/
   SamplePeriod = time_us_64() + (1000000 / WAV_SAMPLE_RATE);
   while (SampleCount < SAMPLES)
   {
  /***************************/
 /* Read values @ 22050 Hz. */
/***************************/
      while(time_us_64() < SamplePeriod);
      SamplePeriod = time_us_64() + (1000000 / WAV_SAMPLE_RATE);
  /****************************************************/
 /* Read the current audio level from A/D converter. */
/****************************************************/
      adc_buffer[SampleCount++] = (int16_t)((adc_read() * 32) - 32768);

   
   }
}





// Callback when data is received
static err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Connection closed
        tcp_close(tpcb);
    } else {
        // Process received data
        tcp_recved(tpcb, p->len); // Acknowledge data reception
        pbuf_free(p); // Free the buffer
    }
    return ERR_OK;
}

// Callback when data is sent
static err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    printf("Data sent successfully.\n");
    return ERR_OK;
}

static err_t connect_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err == ERR_OK) {
        printf("Conexão estabelecida com sucesso!\n");
    } else {
        printf("Erro durante a conexão: %d\n", err);
    }
    return ERR_OK;
}


// Callback when an error occurs
static void error_callback(void *arg, err_t err) {
    printf("Connection error: %d\n", err);
}

void send_to_server(uint16_t *data, size_t len) {
    ip_addr_t server_ip;
    IP4_ADDR(&server_ip, 192, 168, 1, 193); // IP do servidor
    static bool is_connected = false; 

    if(!is_connected)
    {
        if (client_pcb != NULL) {
        printf("PCB antigo encontrado. Fechando...\n");
        tcp_close(client_pcb);
        client_pcb = NULL;
        }


        client_pcb = tcp_new();
        if (client_pcb == NULL) {
            printf("Erro ao criar PCB.\n");
            return;
        }

        printf("PCB criado com sucesso.\n");

        tcp_arg(client_pcb, data); // Passar dados para o callback
        tcp_err(client_pcb, error_callback);

        err_t err = tcp_connect(client_pcb, &server_ip, SERVER_PORT, connect_callback);
        if (err != ERR_OK) {
            printf("Erro ao iniciar conexão: %d\n", err);
            tcp_close(client_pcb);
        }

        is_connected = true;
    }
   
    else
    {
        printf("envio de dados\n");

        // Obter os dados de áudio e seu comprimento
        //uint16_t *data = (uint16_t *)arg;
        //size_t len = SAMPLES;

        // Calcular o tamanho total em bytes dos dados de áudio (2 bytes por amostra)
        size_t data_size = len * sizeof(uint16_t);

        // Criar um buffer para os dados a serem enviados
        char *send_buffer = malloc(4 + data_size); // 4 bytes para o tamanho + dados binários
        if (send_buffer == NULL) {
            printf("Erro ao alocar memória para o buffer.\n");
            return;
        }

        // Adicionar o tamanho dos dados (4 bytes no início)
        send_buffer[0] = (data_size >> 24) & 0xFF;
        send_buffer[1] = (data_size >> 16) & 0xFF;
        send_buffer[2] = (data_size >> 8) & 0xFF;
        send_buffer[3] = data_size & 0xFF;

        // Copiar os dados de áudio para o buffer após os 4 bytes iniciais
        memcpy(send_buffer + 4, data, data_size);

        // Enviar os dados
        err_t write_err = tcp_write(client_pcb, send_buffer, 4 + data_size, TCP_WRITE_FLAG_COPY);
        if (write_err != ERR_OK) {
            printf("Erro ao enviar dados: %d\n", write_err);
            free(send_buffer);
            return;
        }

        printf("Dados enviados (%lu bytes).\n", 4 + data_size);

        // Garantir que os dados sejam transmitidos
        tcp_output(client_pcb);
        free(send_buffer);

        // Configurar callbacks adicionais
        tcp_recv(client_pcb, recv_callback);
        tcp_sent(client_pcb, sent_callback);

    
    }

    

}




int main() {
  stdio_init_all();

  // Delay para o usuário abrir o monitor serial...
  sleep_ms(1000);

  // Configurar Wi-Fi
  if (cyw43_arch_init()) {
      printf("Falha ao inicializar Wi-Fi.\n");
      return -1;
  }
  cyw43_arch_enable_sta_mode();

  const char *ssid = "LIVE TIM_7660_2G";
  const char *password = "Z248ZmXH";
  printf("Conectando ao Wi-Fi...\n");
  if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
      printf("Falha ao conectar ao Wi-Fi.\n");
      return -1;
  }
  printf("Wi-Fi conectado!\n");



  // Preparação do ADC.
  printf("Preparando ADC...\n");

  adc_gpio_init(MIC_PIN);
  adc_init();
  adc_select_input(MIC_CHANNEL);

  /*adc_fifo_setup(
    true, // Habilitar FIFO
    true, // Habilitar request de dados do DMA
    1, // Threshold para ativar request DMA é 1 leitura do ADC
    false, // Não usar bit de erro
    false // Não fazer downscale das amostras para 8-bits, manter 12-bits.
  );*/

  adc_set_clkdiv(ADC_CLOCK_DIV);

  printf("ADC Configurado!\n\n");

  printf("Preparando DMA...");

  // Tomando posse de canal do DMA.
  /*dma_channel = dma_claim_unused_channel(true);

  // Configurações do DMA.
  dma_cfg = dma_channel_get_default_config(dma_channel);

  channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16); // Tamanho da transferência é 16-bits (usamos uint16_t para armazenar valores do ADC)
  channel_config_set_read_increment(&dma_cfg, false); // Desabilita incremento do ponteiro de leitura (lemos de um único registrador)
  channel_config_set_write_increment(&dma_cfg, true); // Habilita incremento do ponteiro de escrita (escrevemos em um array/buffer)
  
  channel_config_set_dreq(&dma_cfg, DREQ_ADC); // Usamos a requisição de dados do ADC
*/
  // Amostragem de teste.
  printf("Amostragem de teste...\n");
  //sample_mic();


  printf("Configuracoes completas!\n");

  printf("\n----\nIniciando loop...\n----\n");


  sleep_ms(5000);

  printf("começando a gravar");

  sample_mic_no_dma();

  printf("termino da gravacao");

  // mandando ao servidor

   for (int i = 0; i < SAMPLES; i += CHUNK_SIZE) {
        // Calcular o tamanho do chunk atual (o último pode ser menor que CHUNK_SIZE)
        size_t current_chunk_size = ((i + CHUNK_SIZE) <= SAMPLES) 
                                    ? CHUNK_SIZE 
                                    : (SAMPLES - i);

        // Passar o ponteiro deslocado para a função send_to_server
        send_to_server(&adc_buffer[i], current_chunk_size);
        sleep_ms(1000);
    }


  while (true) {

    // Realiza uma amostragem do microfone.
    //sample_mic_no_dma();

    

     //send_to_server(adc_buffer, SAMPLES);
    
    //sleep_ms(100);
  }

  cyw43_arch_deinit();
  return 0;

}



