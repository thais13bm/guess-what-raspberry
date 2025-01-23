#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "neopixel.c"
#include "pico/cyw43_arch.h"   //e se eu botar o path completo?
//#include "pico/cyw43_arch_lwip.h"  //acho que esse real nao tem
//#include "lwip/sockets.h"
//#include "lwip/inet.h"
#include "lwip/tcp.h"



#define SERVER_IP "192.168.1.193" // Substitua pelo IP do servidor Flask
#define SERVER_PORT 5000

// Pino e canal do microfone no ADC.
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)

// Parâmetros e macros do ADC.
#define ADC_CLOCK_DIV 96.f
//#define ADC_CLOCK_DIV 3000.f  //novo teste

//#define SAMPLES 200 // Número de amostras que serão feitas do ADC.

#define SAMPLES 80000

#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f) // Ajuste do valor do ADC para Volts.
#define ADC_MAX 3.3f
#define ADC_STEP (3.3f/5.f) // Intervalos de volume do microfone.

// Pino e número de LEDs da matriz de LEDs.
#define LED_PIN 7
#define LED_COUNT 25

#define BUFFER_SIZE 1024 

//#define BUFFER_SIZE 510000

#define abs(x) ((x < 0) ? (-x) : (x))

// Canal e configurações do DMA
uint dma_channel;
dma_channel_config dma_cfg;

// Buffer de amostras do ADC.
uint16_t adc_buffer[SAMPLES];
static struct tcp_pcb *client_pcb;


//declaracao de funcoes

/**
 * Realiza as leituras do ADC e armazena os valores no buffer.
 */
void sample_mic() {
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
}

/**
 * Calcula a potência média das leituras do ADC. (Valor RMS)
 */
float mic_power() {
  float avg = 0.f;

  for (uint i = 0; i < SAMPLES; ++i)
    avg += adc_buffer[i] * adc_buffer[i];
  
  avg /= SAMPLES;
  return sqrt(avg);
}

/**
 * Calcula a intensidade do volume registrado no microfone, de 0 a 4, usando a tensão.
 */
uint8_t get_intensity(float v) {
  uint count = 0;

  while ((v -= ADC_STEP/20) > 0.f)
    ++count;
  
  return count;
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
    
    printf("to no connect callback\n");
    uint16_t *data = (uint16_t *)arg;
    size_t len = SAMPLES;


    // Serializar os dados como JSON
    char json_buffer[BUFFER_SIZE];
    snprintf(json_buffer, BUFFER_SIZE, "{\"audio\":[");
    for (size_t i = 0; i < len; ++i) {
        char sample[16];
        snprintf(sample, sizeof(sample), "%d%s", data[i], (i == len - 1) ? "]}" : ",");
        strncat(json_buffer, sample, sizeof(json_buffer) - strlen(json_buffer) - 1);
    }

    // Calcular o tamanho do JSON
    size_t json_len = strlen(json_buffer);

    // Criar um buffer para os dados a serem enviados
    char *send_buffer = malloc(4 + json_len); // 4 bytes para o tamanho + JSON
    if (send_buffer == NULL) {
        printf("Erro ao alocar memória para o buffer.\n");
        return ERR_MEM;
    }

    // Adicionar o tamanho do JSON (4 bytes) no início
    send_buffer[0] = (json_len >> 24) & 0xFF;
    send_buffer[1] = (json_len >> 16) & 0xFF;
    send_buffer[2] = (json_len >> 8) & 0xFF;
    send_buffer[3] = json_len & 0xFF;

    // Copiar o JSON para o buffer
    memcpy(send_buffer + 4, json_buffer, json_len);

    // Enviar os dados
    err_t write_err = tcp_write(tpcb, send_buffer, 4 + json_len, TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("Erro ao enviar dados: %d\n", write_err);
        free(send_buffer);
        return write_err;
    }

    printf("JSON enviado: %s\n", send_buffer);

    tcp_output(tpcb);
    free(send_buffer);

    // Configurar callbacks adicionais
    tcp_recv(tpcb, recv_callback);
    tcp_sent(tpcb, sent_callback);

    return ERR_OK;



    /*char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "{\"data\":[");
    for (size_t i = 0; i < len; ++i) {
        char sample[16];
        snprintf(sample, sizeof(sample), "%d%s", data[i], (i == len - 1) ? "]}" : ",");
        strncat(buffer, sample, sizeof(buffer) - strlen(buffer) - 1);
    }

    tcp_write(tpcb, buffer, strlen(buffer), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    // Definir callbacks adicionais
    tcp_recv(tpcb, recv_callback);
    tcp_sent(tpcb, sent_callback);
    return ERR_OK;*/
}


// Callback when an error occurs
static void error_callback(void *arg, err_t err) {
    printf("Connection error: %d\n", err);
}

void send_to_server(uint16_t *data, size_t len) {
    ip_addr_t server_ip;
    IP4_ADDR(&server_ip, 192, 168, 1, 193); // IP do servidor


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

   

    //tcp_close(client_pcb);
    //client_pcb = NULL;
    
    
    /*tcp_err(client_pcb, error_callback);

    err_t err = tcp_connect(client_pcb, &server_ip, SERVER_PORT, connect_callback);
    if (err != ERR_OK) {
        printf("Erro ao iniciar conexão: %d\n", err);
        tcp_close(client_pcb);
        client_pcb = NULL;
        return;
    }

    // Criar o payload JSON
    char payload[BUFFER_SIZE];
    snprintf(payload, BUFFER_SIZE, "{\"audio\":[");
    for (size_t i = 0; i < len; ++i) {
        char sample[16];
        snprintf(sample, sizeof(sample), "%d%s", data[i], (i == len - 1) ? "]}" : ",");
        strncat(payload, sample, sizeof(payload) - strlen(payload) - 1);
    }

    // Montar a requisição HTTP
    char request[BUFFER_SIZE];
    snprintf(request, BUFFER_SIZE,
             "POST /upload HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "\r\n"
             "%s",
             SERVER_IP, SERVER_PORT, strlen(payload), payload);

    // Enviar a requisição HTTP pelo TCP
    tcp_write(client_pcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    tcp_output(client_pcb);

    printf("Dados enviados ao servidor:\n%s\n", request);*/

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


  // Preparação da matriz de LEDs.
  //printf("Preparando NeoPixel...");
  
  //npInit(LED_PIN, LED_COUNT);

  // Preparação do ADC.
  printf("Preparando ADC...\n");

  adc_gpio_init(MIC_PIN);
  adc_init();
  adc_select_input(MIC_CHANNEL);

  adc_fifo_setup(
    true, // Habilitar FIFO
    true, // Habilitar request de dados do DMA
    1, // Threshold para ativar request DMA é 1 leitura do ADC
    false, // Não usar bit de erro
    false // Não fazer downscale das amostras para 8-bits, manter 12-bits.
  );

  adc_set_clkdiv(ADC_CLOCK_DIV);

  printf("ADC Configurado!\n\n");

  printf("Preparando DMA...");

  // Tomando posse de canal do DMA.
  dma_channel = dma_claim_unused_channel(true);

  // Configurações do DMA.
  dma_cfg = dma_channel_get_default_config(dma_channel);

  channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16); // Tamanho da transferência é 16-bits (usamos uint16_t para armazenar valores do ADC)
  channel_config_set_read_increment(&dma_cfg, false); // Desabilita incremento do ponteiro de leitura (lemos de um único registrador)
  channel_config_set_write_increment(&dma_cfg, true); // Habilita incremento do ponteiro de escrita (escrevemos em um array/buffer)
  
  channel_config_set_dreq(&dma_cfg, DREQ_ADC); // Usamos a requisição de dados do ADC

  // Amostragem de teste.
  printf("Amostragem de teste...\n");
  sample_mic();


  printf("Configuracoes completas!\n");

  printf("\n----\nIniciando loop...\n----\n");

  while (true) {

    // Realiza uma amostragem do microfone.
    sample_mic();

    // Pega a potência média da amostragem do microfone.
    /*float avg = mic_power();
    avg = 2.f * abs(ADC_ADJUST(avg)); // Ajusta para intervalo de 0 a 3.3V. (apenas magnitude, sem sinal)

    uint intensity = get_intensity(avg); // Calcula intensidade a ser mostrada na matriz de LEDs.

    // Limpa a matriz de LEDs.
    npClear();*/
    //printf("%d",adc_buffer[0]);


    // Envia a intensidade e a média das leituras do ADC por serial.
    //printf("%2d %8.4f\r", intensity, avg);

    /*ip_addr_t test_ip;
    IP4_ADDR(&test_ip, 192, 168, 1, 193); // IP do servidor
    struct tcp_pcb *test_pcb = tcp_new();
    if (test_pcb == NULL) {
        printf("Falha ao criar PCB de teste.\n");
    } else {
        printf("PCB de teste criado com sucesso.\n");
        tcp_close(test_pcb);
    }*/


    send_to_server(adc_buffer, SAMPLES);
    sleep_ms(1000);
  }

  cyw43_arch_deinit();
  return 0;

}





/*void send_to_server(uint16_t *data, size_t len) {
    int sock;
    struct sockaddr_in server_addr;

    // Configurar o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Criar o socket
    sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Erro ao criar socket\n");
        return;
    }

    // Conectar ao servidor
    if (lwip_connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Erro ao conectar ao servidor\n");
        lwip_close(sock);
        return;
    }

    // Criar a string de dados em JSON
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "{\"data\":[");
    for (size_t i = 0; i < len; ++i) {
        char sample[16];
        snprintf(sample, sizeof(sample), "%d%s", data[i], (i == len - 1) ? "]}" : ",");
        strncat(buffer, sample, sizeof(buffer) - strlen(buffer) - 1);
    }

    // Enviar dados ao servidor
    lwip_write(sock, buffer, strlen(buffer));
    lwip_close(sock);
    printf("Dados enviados para o servidor.\n");
}*/
