
/**
 * AULA IoT - Embarcatech - Ricardo Prates - 004 - Webserver Raspberry Pi Pico w - wlan
 *
 * Material de suporte
 *
 * https://www.raspberrypi.com/documentation/pico-sdk/networking.html#group_pico_cyw43_arch_1ga33cca1c95fc0d7512e7fef4a59fd7475
 */

#include <stdio.h>           // Biblioteca padrão para entrada e saída
#include <string.h>          // Biblioteca manipular strings
#include <stdlib.h>          // funções para realizar várias operações, incluindo alocação de memória dinâmica (malloc)
#include "pico/stdlib.h"     // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "hardware/adc.h"    // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC
#include "pico/cyw43_arch.h" // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "lwip/pbuf.h"       // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"        // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h"      // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)
#include "lib/matrizRGB.h"
#include "lib/ssd1306.h"

// Credenciais WIFI - Tome cuidado se publicar no github!
#define WIFI_SSID "RaGus2.5GHZ"
#define WIFI_PASSWORD "#RaGus2.5GHZ6258"

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN // GPIO do CI CYW43
#define LED_BLUE_PIN 12               // GPIO12 - LED azul
#define LED_GREEN_PIN 11              // GPIO11 - LED verde
#define LED_RED_PIN 13                // GPIO13 - LED vermelho
static const uint VRY_PIN = 27;
static const uint VRX_PIN = 26;
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define I2C_ADDR 0x3C
static ssd1306_t ssd;

volatile int sala_estado = 0;
volatile int cozinha_estado = 0;
volatile int quarto_estado = 0;
volatile int banheiro_estado = 0;

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(void);

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Leitura da temperatura interna
float temp_read(void);

// Tratamento do request do usuário
void user_request(char **request);

void init_i2c()
{
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void init_display()
{
    ssd1306_init(&ssd, 128, 64, false, I2C_ADDR, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
}

void init_joystick_adc()
{
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);
}

// Função principal
int main()
{
    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

    // Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
    gpio_led_bitdog();

    init_i2c();
    init_display();
    init_joystick_adc();
    npInit(7);

    // Inicializa a arquitetura do cyw43
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    // GPIO do CI CYW43 em nível baixo
    cyw43_arch_gpio_put(LED_PIN, 0);

    // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
    cyw43_arch_enable_sta_mode();

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }
    printf("Conectado ao Wi-Fi\n");

    // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    // vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
    server = tcp_listen(server);

    // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
    tcp_accept(server, tcp_server_accept);
    printf("Servidor ouvindo na porta 80\n");

    // Inicializa o conversor ADC
    adc_init();
    adc_set_temp_sensor_enabled(true);

    while (true)
    {
        /*
         * Efetuar o processamento exigido pelo cyw43_driver ou pela stack TCP/IP.
         * Este método deve ser chamado periodicamente a partir do ciclo principal
         * quando se utiliza um estilo de sondagem pico_cyw43_arch
         */
        cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo
        sleep_ms(100);     // Reduz o uso da CPU
    }

    // Desligar a arquitetura CYW43.
    cyw43_arch_deinit();
    return 0;
}

// -------------------------------------- Funções ---------------------------------

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(void)
{
    // Configuração dos LEDs como saída
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);

    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);

    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Tratamento do request do usuário - digite aqui
void user_request(char **request)
{
    if (strstr(*request, "GET /sala_on") != NULL)
    {
        sala_estado = 1;
        ssd1306_fill(&ssd, false);
        ssd1306_send_data(&ssd);
        gpio_put(LED_BLUE_PIN, 1);
        npSetLED(0, 0, COLOR_BLUE); // Azul
        npSetLED(0, 1, COLOR_BLUE); // Azul
        npSetLED(1, 0, COLOR_BLUE); // Azul
        npSetLED(1, 1, COLOR_BLUE); // Azul
        npWrite();
    }
    else if (strstr(*request, "GET /sala_off") != NULL)
    {
        gpio_put(LED_BLUE_PIN, 0);
        npSetLED(0, 0, COLOR_BLACK); // DESLIGADO
        npSetLED(0, 1, COLOR_BLACK); // DESLIGADO
        npSetLED(1, 0, COLOR_BLACK); // DESLIGADO
        npSetLED(1, 1, COLOR_BLACK); // Azul
        npWrite();
        sala_estado = 0;
    }
    else if (strstr(*request, "GET /cozinha_on") != NULL)
    {
        gpio_put(LED_RED_PIN, 1);
        npSetLED(3, 0, COLOR_RED); // Azul
        npSetLED(4, 0, COLOR_RED); // Azul
        npSetLED(3, 1, COLOR_RED); // Azul
        npSetLED(4, 1, COLOR_RED); // Azul
        npWrite();

        cozinha_estado = 1;
    }
    else if (strstr(*request, "GET /cozinha_off") != NULL)
    {
        gpio_put(LED_RED_PIN, 0);
        npSetLED(3, 0, COLOR_BLACK); // Azul
        npSetLED(4, 0, COLOR_BLACK); // Azul
        npSetLED(3, 1, COLOR_BLACK); // Azul
        npSetLED(4, 1, COLOR_BLACK); // Azul
        npWrite();
        cozinha_estado = 0;
    }
    else if (strstr(*request, "GET /quarto_on") != NULL)
    {
        gpio_put(LED_GREEN_PIN, 1);
        npSetLED(0, 3, COLOR_GREEN); // Azul
        npSetLED(0, 4, COLOR_GREEN); // Azul
        npSetLED(1, 3, COLOR_GREEN); // Azul
        npSetLED(1, 4, COLOR_GREEN); // Azul
        npWrite();
        quarto_estado = 1;
    }
    else if (strstr(*request, "GET /quarto_off") != NULL)
    {
        gpio_put(LED_GREEN_PIN, 0);
        npSetLED(0, 3, COLOR_BLACK); // Azul
        npSetLED(0, 4, COLOR_BLACK); // Azul
        npSetLED(1, 3, COLOR_BLACK); // Azul
        npSetLED(1, 4, COLOR_BLACK); // Azul
        npWrite();
        quarto_estado = 0;
    }
    else if (strstr(*request, "GET /banheiro_on") != NULL)
    {

        npSetLED(3, 3, COLOR_VIOLET); // Azul
        npSetLED(3, 4, COLOR_VIOLET); // Azul
        npSetLED(4, 3, COLOR_VIOLET); // Azul
        npSetLED(4, 4, COLOR_VIOLET); // Azul
        npWrite();
        banheiro_estado = 1;
    }
    else if (strstr(*request, "GET /banheiro_off") != NULL)
    {
        gpio_put(LED_GREEN_PIN, 0);
        npSetLED(3, 3, COLOR_BLACK); // Azul
        npSetLED(3, 4, COLOR_BLACK); // Azul
        npSetLED(4, 3, COLOR_BLACK); // Azul
        npSetLED(4, 4, COLOR_BLACK); // Azul
        npWrite();
        banheiro_estado = 0;
    }
}

// Leitura da temperatura interna
float temp_read(void)
{
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    float temperature = 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
    return temperature;
}

float umidade_read(void)
{
    adc_select_input(0);
    uint16_t adc_y = adc_read();
    printf("ADC Y: %d\n", adc_y); // Para debug

    // Valor padrão do joystick (posição neutra) e limites
    uint16_t baseline = 1997;
    uint16_t margem_erro = 50;
    uint16_t valor_min = 11;
    uint16_t valor_max = 4073;

    // Calcular umidade relativa (%)
    // Considerando que o ponto central (baseline) é aproximadamente 50% de umidade
    // e os extremos representam 0% e 100%
    float umidade;

    if (adc_y <= valor_min)
    {
        umidade = 0.0;
    }
    else if (adc_y >= valor_max)
    {
        umidade = 100.0;
    }
    else if (adc_y < baseline)
    {
        // Valor abaixo do baseline (0% a 50%)
        umidade = 50.0 * ((float)(adc_y - valor_min) / (baseline - valor_min));
    }
    else
    {
        // Valor acima do baseline (50% a 100%)
        umidade = 50.0 + 50.0 * ((float)(adc_y - baseline) / (valor_max - baseline));
    }

    printf("Umidade relativa: %.1f%%\n", umidade);
    return umidade;
}

const char *qualidade_ar_read(void)
{
    adc_select_input(1);
    uint16_t adc_x = adc_read();
    printf("ADC X: %d\n", adc_x); // Para debug

    // Valor padrão do joystick (posição neutra)
    uint16_t baseline = 2050;
    uint16_t margem_erro = 50;
    uint16_t valor_min = 11;
    uint16_t valor_max = 4073;

    // Calculando o desvio da posição neutra (INVERTIDO: negativo = mais poluição)
    int16_t delta = adc_x - baseline;
    printf("Delta qualidade: %d\n", delta);

    // Classificação baseada no desvio da posição central COM LÓGICA INVERTIDA
    if (delta < -1000)
    {
        // Adc_x < 1050 (muito desviado para esquerda)
        return "RUIM";
    }
    else if (delta < -400)
    {
        // Adc_x entre 1650 e 1050
        return "MODERADA";
    }
    else if (abs(delta) <= margem_erro)
    {
        // Dentro da margem de erro do ponto central (neutro)
        return "BOA";
    }
    else if (delta > 400)
    {
        // Adc_x > 2450 (muito para direita)
        return "EXCELENTE";
    }
    else
    {
        // Para pequenos desvios positivos
        return "BOA";
    }
}

void update_display(ssd1306_t *ssd, float temperatura, int umidade, const char *qualidade_ar)
{
    // Limpa o display
    ssd1306_fill(ssd, false);
    
    // Temperatura na primeira linha
    char temp_str[32];
    sprintf(temp_str, "Temp: %.1f C", temperatura);
    ssd1306_draw_string(ssd, temp_str, 0, 5);
    
    // Umidade na segunda linha
    char umid_str[32];
    sprintf(umid_str, "Umidade: %d%%", umidade);
    ssd1306_draw_string(ssd, umid_str, 0, 25);
    
    // Qualidade do ar na terceira linha
    char qual_str[32];
    sprintf(qual_str, "Qualidade: %s", qualidade_ar);
    ssd1306_draw_string(ssd, qual_str, 0, 45);
    
    // Atualiza o display
    ssd1306_send_data(ssd);
}

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Tratamento de request - Controle dos LEDs
    user_request(&request);

    // Leitura da temperatura interna
    float temperatura = temp_read();
    int umidade = umidade_read();
    const char *qualidade_ar = qualidade_ar_read();

    update_display(&ssd, temperatura, umidade, qualidade_ar);

    // Cria a resposta HTML
    char html[1024];

    // Instruções html do webserver
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<meta charset=\"UTF-8\">\n"
             "<meta name=\"viewport\" content=\"width=device-width\">\n"
             "<title>Automação</title>\n"
             "<style>\n"
             "body{text-align:center}\n"
             ".btn{margin:5px;padding:15px 25px;cursor:pointer}\n"
             ".s{margin:10px 0;padding:10px;border:1px solid #ccc;background-color:#fff;border-radius:5px}\n"
             "form{display:inline-block}\n"
             "</style>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Automação Residencial</h1>\n"
             "<h2>Iluminação</h2>\n"
             "<form action=\"/sala_%s\"><button class=\"btn %s\">Sala: %s</button></form>\n"
             "<form action=\"/cozinha_%s\"><button class=\"btn %s\">Cozinha: %s</button></form>\n"
             "<form action=\"/quarto_%s\"><button class=\"btn %s\">Quarto: %s</button></form>\n"
             "<form action=\"/banheiro_%s\"><button class=\"btn %s\">Banheiro: %s</button></form>\n"
             "<h2>Sensores</h2>\n"
             "<div class=\"s\">Temperatura: %.1f &deg;C</div>\n"
             "<div class=\"s\">Umidade: %d </div>\n"
             "<div class=\"s\">Qualidade do Ar: %s</div>\n"
             "</body>\n"
             "</html>\n",
             sala_estado ? "off" : "on", sala_estado ? "active" : "", sala_estado ? "Ligada" : "Desligada",
             cozinha_estado ? "off" : "on", cozinha_estado ? "active" : "", cozinha_estado ? "Ligada" : "Desligada",
             quarto_estado ? "off" : "on", quarto_estado ? "active" : "", quarto_estado ? "Ligada" : "Desligada",
             banheiro_estado ? "off" : "on", banheiro_estado ? "active" : "", banheiro_estado ? "Ligada" : "Desligada",
             temperatura, umidade, qualidade_ar);

    // Escreve dados para envio (mas não os envia imediatamente).
    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

    // Envia a mensagem
    tcp_output(tpcb);

    // libera memória alocada dinamicamente
    free(request);

    // libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}
