#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"  
#include "lwip/tcp.h"
#include <stdint.h>
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15

#define CHAR_WIDTH 10  // Largura estimada de um caractere
#define LINE_HEIGHT 8  // Altura da linha em pixels
#define MAX_WIDTH ssd1306_width  

#define WAV_SAMPLE_RATE  22050
#define WAV_PWM_COUNT   (125000000 / WAV_SAMPLE_RATE)


#define SERVER_PORT 5000

// Pino e canal do microfone no ADC.
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)


// Parâmetros e macros do ADC.
#define ADC_CLOCK_DIV 2177.28



#define CHUNK_SIZE 1200
#define AUDIO_PERIOD    3
#define SAMPLES      (WAV_SAMPLE_RATE * AUDIO_PERIOD)  




#define GREEN_LED_PIN 11
#define BLUE_LED_PIN 12
#define RECORD_BTN  5




unsigned long sample_count = 0;
unsigned long long sample_period;
uint16_t adc_buffer[SAMPLES];
static struct tcp_pcb *client_pcb;



char send_buffer[2400];


static char recv_buffer[16]; // Buffer para armazenar dados recebidos
static size_t recv_buffer_len = 0;
char *data = NULL; 
size_t data_len;

uint16_t listen_flag = 1; 
bool is_connected = false;
volatile bool is_recording = false;



//declaracao de funcoes



void sample_mic_no_dma()
{
   
   sample_count = 0;
   sample_period = time_us_64() + (1000000 / WAV_SAMPLE_RATE);

   while (sample_count < SAMPLES)
   {
  
      while(time_us_64() < sample_period);
      sample_period = time_us_64() + (1000000 / WAV_SAMPLE_RATE);
      adc_buffer[sample_count++] = (int16_t)((adc_read() * 32) - 32768);

   }
}




// Callback para o recebimento de dados
static err_t recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Conexão fechada pelo cliente
        printf("Conexão encerrada pelo servidor.\n");
        tcp_close(tpcb);
        return ERR_OK;
    }

    if (err != ERR_OK) {
        printf("Erro no callback de recepção: %d\n", err);
        pbuf_free(p);
        return err;
    }

    // Verifica o tamanho do buffer recebido
    size_t len = p->len;
    if (len > 0) {
        if ((recv_buffer_len + len) < sizeof(recv_buffer)) {
            // Copia os dados para o buffer global
            memcpy(recv_buffer + recv_buffer_len, p->payload, len);
            recv_buffer_len += len;
            printf("Recebi %zu bytes. Total acumulado: %zu bytes.\n", len, recv_buffer_len);
        } else {
            printf("Buffer de recepção está cheio. Dados descartados.\n");
        }
    }

    // Libera o pbuf após processar os dados
    pbuf_free(p);

    // Confirma o recebimento ao servidor
    tcp_recved(tpcb, len);

    return ERR_OK;
}



// Callback para o envio de dados
static err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    printf("Dados enviados.\n");
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


// Callback de erro
static void error_callback(void *arg, err_t err) {
    printf("erro de conexão: %d\n", err);
}


void connect_to_server(){
    ip_addr_t server_ip;
    IP4_ADDR(&server_ip, 0, 0, 0, 0); // IP do servidor
   

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

        // Configurar callbacks adicionais
        tcp_recv(client_pcb, recv_callback);
        tcp_sent(client_pcb, sent_callback);

        
        tcp_err(client_pcb, error_callback);

        err_t err = tcp_connect(client_pcb, &server_ip, SERVER_PORT, connect_callback);
        if (err != ERR_OK) {
            printf("Erro ao iniciar conexão: %d\n", err);
            tcp_close(client_pcb);
        }

        is_connected = true;
    }
}



void send_to_server(uint16_t *data, size_t len) {
    ip_addr_t server_ip;
    IP4_ADDR(&server_ip, 0, 0, 0, 0); // IP do servidor
    
    tcp_arg(client_pcb, data); // Passar dados para o callback
    if(is_connected)
    {
        printf("envio de dados\n");

        
        // Calcular o tamanho total em bytes dos dados de áudio (2 bytes por amostra)
        size_t data_size = len * sizeof(uint16_t);

        

        

        // Copiar os dados de áudio para o buffer após os 4 bytes iniciais
        memcpy(send_buffer , data, data_size);

        // Verificar espaço disponível no buffer TCP
        uint16_t available_space = tcp_sndbuf(client_pcb);
        if ((data_size) > available_space) {
            printf("Espaço insuficiente no buffer TCP: %d bytes disponíveis, %lu bytes necessários\n",
                available_space, data_size);
            
            return; // Retornar para evitar tentar o envio
        }


        // Enviar os dados
        err_t write_err = tcp_write(client_pcb, send_buffer, data_size, TCP_WRITE_FLAG_COPY);
        if (write_err != ERR_OK) {
            //printf("Erro ao enviar dados: %d\n", write_err);
          
            return;
        }

       

        

        printf("Dados enviados (%lu bytes).\n", data_size);

        
    }
    
}

char* get_received_data(size_t *data_len) {
    if (recv_buffer_len > 0) {
        *data_len = recv_buffer_len;
        recv_buffer_len = 0; // Reseta o comprimento após retornar os dados
        return recv_buffer;
    } else {
        *data_len = 0;
        return NULL;
    }
}


void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == RECORD_BTN)
    {
        is_recording = !is_recording;
    }
}




int main() {
  stdio_init_all();

  // Delay para o usuário abrir o monitor serial...
  sleep_ms(1000);

  gpio_init(GREEN_LED_PIN);
  gpio_set_dir(GREEN_LED_PIN, GPIO_OUT);  
  gpio_put(GREEN_LED_PIN, 0);  

  gpio_init(BLUE_LED_PIN);
  gpio_set_dir(BLUE_LED_PIN, GPIO_OUT);  
  gpio_put(BLUE_LED_PIN, 0);



  gpio_init(RECORD_BTN);
  gpio_set_dir(RECORD_BTN,GPIO_IN);
  gpio_pull_up(RECORD_BTN);

  // Configurar interrupção no botão (queda de borda)
  gpio_set_irq_enabled_with_callback(RECORD_BTN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);




  // Configurar Wi-Fi
  if (cyw43_arch_init()) {
      printf("Falha ao inicializar Wi-Fi.\n");
      return -1;
  }
  cyw43_arch_enable_sta_mode();

 

  const char *ssid = "nome da sua rede wifi";
  const char *password = "senha da sua rede wifi";

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

 

  adc_set_clkdiv(ADC_CLOCK_DIV);

  printf("ADC Configurado!\n\n");
     

  connect_to_server();
  
  
  gpio_put(BLUE_LED_PIN,1);

  // Inicialização do i2c
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Processo de inicialização completo do OLED SSD1306
    ssd1306_init();

    // Preparar área de renderização para o display (ssd1306_width pixels por ssd1306_n_pages páginas)
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

    // zera o display inteiro
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

   

  while (true) {

        

        if(is_recording)
        {    
            printf("Iniciando gravação...\n");
            gpio_put(BLUE_LED_PIN,0);

            sleep_ms(1000); //para nao gravar o barulho do botao
            gpio_put(GREEN_LED_PIN,1);

            sample_mic_no_dma();

            gpio_put(GREEN_LED_PIN,0);

            printf("termino da gravacao");

            // mandando ao servidor
           
            ssd1306_draw_string(ssd, 5, 24, "Enviando audio");
            
            render_on_display(ssd, &frame_area);




            for (int i = 0; i < SAMPLES; i += CHUNK_SIZE) {
                // Calcular o tamanho do chunk atual (o último pode ser menor que CHUNK_SIZE)
                size_t current_chunk_size = ((i + CHUNK_SIZE) <= SAMPLES) 
                                            ? CHUNK_SIZE 
                                            : (SAMPLES - i);

                // Passar o ponteiro deslocado para a função send_to_server
                send_to_server(&adc_buffer[i], current_chunk_size);
                sleep_ms(500);
            }

            sleep_ms(1000);    

            send_to_server(&listen_flag, 1); // Envia a flag ao servidor 
            
            memset(ssd, 0, ssd1306_buffer_length);
            render_on_display(ssd, &frame_area);

            while(true){

                data = get_received_data(&data_len);

                printf("%d",data_len);

                if (data_len > 0)
                {                    
                    uint x = 5;  // Posição inicial x
                    uint y = 0;  // Posição inicial y

                    for (uint i = 0; i < data_len; i++)
                    {   
                        if (x + CHAR_WIDTH > MAX_WIDTH) 
                        {
                            // Quebra de linha: Reinicia x e move para a próxima linha
                            x = 5;
                            y += LINE_HEIGHT;
                        }

                        printf("%c", data[i]);
                        ssd1306_draw_char(ssd, x, y, data[i]);
                        x += CHAR_WIDTH; // Avança para a próxima posição horizontal
                    }
                    
                    render_on_display(ssd, &frame_area);   
                    break;
                }
            sleep_ms(100);
            }


            sleep_ms(8000); 
            


            gpio_put(BLUE_LED_PIN,1);
        }
    


    sleep_ms(100);  

    
        

  }

  cyw43_arch_deinit();
  return 0;

}



