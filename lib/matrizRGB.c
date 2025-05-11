/**
 * @file matrizRGB.c
 * @brief Implementação das funções para controle de matriz 5x5 de LEDs RGB
 *
 * Esta implementação utiliza o hardware PIO do Raspberry Pi Pico para
 * controlar uma matriz de LEDs WS2812B (NeoPixels).
 *
 * @author [Aulo Cezar]
 * @date [01/05/2025]
 */

#include "matrizRGB.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2818b.pio.h" // Arquivo gerado pelo compilador PIO

/* Estado global da matriz de LEDs */
npLED_t leds[NP_LED_COUNT];

/* Cores predefinidas acessíveis externamente */
const npColor_t npColors[] = {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE, COLOR_BLACK,
    COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_PURPLE, COLOR_ORANGE,
    COLOR_BROWN, COLOR_VIOLET, COLOR_GREY, COLOR_GOLD, COLOR_SILVER};

/* Estado interno do hardware PIO */
static PIO np_pio = NULL; // Instância PIO utilizada
static uint sm = 0;       // State Machine utilizada

/**
 * @brief Converte coordenadas (x,y) para índice no array linear de LEDs
 *
 * A função leva em conta o padrão de "zig-zag" típico de matrizes WS2812B,
 * onde linhas alternadas têm sentidos opostos.
 *
 * @param x Coordenada horizontal (0-4)
 * @param y Coordenada vertical (0-4)
 * @return Índice correspondente no array linear
 */
static int getIndex(int x, int y)
{
    // Calculamos o índice considerando que a matriz é conectada em zigzag
    // As linhas pares vão da esquerda para a direita, as ímpares da direita para a esquerda
    if (y % 2 == 0)
        return (NP_LED_COUNT - 1) - (y * NP_MATRIX_WIDTH + x);
    return (NP_LED_COUNT - 1) - (y * NP_MATRIX_WIDTH + (NP_MATRIX_WIDTH - 1 - x));
}

/**
 * @brief Limita um valor float entre 0.0 e 1.0
 *
 * @param value Valor a ser limitado
 * @return Valor limitado entre 0.0 e 1.0
 */
static inline float clampIntensity(float value)
{
    return (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f
                                                  : value;
}

void npInit(uint8_t pin)
{
    // Adiciona o programa PIO à memória do PIO
    uint offset = pio_add_program(pio0, &ws2818b_program);

    // Tenta usar o PIO0 primeiro, se não estiver disponível usa o PIO1
    np_pio = pio0;
    sm = pio_claim_unused_sm(np_pio, false);

    if (sm == -1) // Se não conseguiu uma state machine no PIO0
    {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Exige que haja uma SM disponível
    }

    // Inicializa o programa PIO com a frequência de 800kHz (padrão WS2812B)
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.0f);

    // Limpa a matriz, iniciando com todos os LEDs apagados
    npClear();
}

void npWrite(void)
{
    // Envia os dados de cada LED para o hardware PIO na ordem correta (GRB)
    for (uint i = 0; i < NP_LED_COUNT; ++i)
    {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
}

void npClear(void)
{
    // Define todos os LEDs como preto (apagados)
    for (uint i = 0; i < NP_LED_COUNT; ++i)
    {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
    npWrite(); // Atualiza o hardware
}

bool npIsPositionValid(int x, int y)
{
    return (x >= 0 && x < NP_MATRIX_WIDTH && y >= 0 && y < NP_MATRIX_HEIGHT);
}

void npSetLED(int x, int y, npColor_t color)
{
    if (npIsPositionValid(x, y))
    {
        uint index = getIndex(x, y);
        leds[index].R = color.r;
        leds[index].G = color.g;
        leds[index].B = color.b;
    }
}

void npSetLEDIntensity(int x, int y, npColor_t color, float intensity)
{
    if (npIsPositionValid(x, y))
    {
        intensity = clampIntensity(intensity);
        uint index = getIndex(x, y);
        leds[index].R = (uint8_t)(color.r * intensity);
        leds[index].G = (uint8_t)(color.g * intensity);
        leds[index].B = (uint8_t)(color.b * intensity);
    }
}

void npSetRow(int row, npColor_t color)
{
    if (row >= 0 && row < NP_MATRIX_HEIGHT)
    {
        for (int x = 0; x < NP_MATRIX_WIDTH; x++)
        {
            npSetLED(x, row, color);
        }
        npWrite(); // Atualiza o hardware
    }
}

void npSetRowIntensity(int row, npColor_t color, float intensity)
{
    if (row >= 0 && row < NP_MATRIX_HEIGHT)
    {
        intensity = clampIntensity(intensity);

        // Pré-calcula os valores com intensidade para evitar cálculos repetidos
        npColor_t adjustedColor = {
            .r = (uint8_t)(color.r * intensity),
            .g = (uint8_t)(color.g * intensity),
            .b = (uint8_t)(color.b * intensity)};

        for (int x = 0; x < NP_MATRIX_WIDTH; x++)
        {
            npSetLED(x, row, adjustedColor);
        }
        npWrite(); // Atualiza o hardware
    }
}

void npSetColumn(int col, npColor_t color)
{
    if (col >= 0 && col < NP_MATRIX_WIDTH)
    {
        for (int y = 0; y < NP_MATRIX_HEIGHT; y++)
        {
            npSetLED(col, y, color);
        }
        npWrite(); // Atualiza o hardware
    }
}

void npSetColumnIntensity(int col, npColor_t color, float intensity)
{
    if (col >= 0 && col < NP_MATRIX_WIDTH)
    {
        intensity = clampIntensity(intensity);

        // Pré-calcula os valores com intensidade para evitar cálculos repetidos
        npColor_t adjustedColor = {
            .r = (uint8_t)(color.r * intensity),
            .g = (uint8_t)(color.g * intensity),
            .b = (uint8_t)(color.b * intensity)};

        for (int y = 0; y < NP_MATRIX_HEIGHT; y++)
        {
            npSetLED(col, y, adjustedColor);
        }
        npWrite(); // Atualiza o hardware
    }
}

void npSetBorder(npColor_t color)
{
    // Define as linhas superiores e inferiores
    for (int x = 0; x < NP_MATRIX_WIDTH; x++)
    {
        npSetLED(x, 0, color);                    // Linha superior
        npSetLED(x, NP_MATRIX_HEIGHT - 1, color); // Linha inferior
    }

    // Define as colunas laterais (excluindo os cantos já definidos)
    for (int y = 1; y < NP_MATRIX_HEIGHT - 1; y++)
    {
        npSetLED(0, y, color);                   // Coluna esquerda
        npSetLED(NP_MATRIX_WIDTH - 1, y, color); // Coluna direita
    }

    npWrite(); // Atualiza o hardware
}

void npSetDiagonal(bool mainDiagonal, npColor_t color)
{
    for (int i = 0; i < NP_MATRIX_WIDTH; i++)
    {
        if (mainDiagonal)
        {
            npSetLED(i, i, color); // Diagonal principal (canto superior esquerdo ao inferior direito)
        }
        else
        {
            npSetLED(NP_MATRIX_WIDTH - 1 - i, i, color); // Diagonal secundária (canto superior direito ao inferior esquerdo)
        }
    }
    npWrite(); // Atualiza o hardware
}

void npFill(npColor_t color)
{
    for (int i = 0; i < NP_LED_COUNT; i++)
    {
        leds[i].R = color.r;
        leds[i].G = color.g;
        leds[i].B = color.b;
    }
    npWrite(); // Atualiza o hardware
}

void npFillIntensity(npColor_t color, float intensity)
{
    intensity = clampIntensity(intensity);

    // Pré-calcula os valores com intensidade para evitar cálculos repetidos
    uint8_t r = (uint8_t)(color.r * intensity);
    uint8_t g = (uint8_t)(color.g * intensity);
    uint8_t b = (uint8_t)(color.b * intensity);

    for (int i = 0; i < NP_LED_COUNT; i++)
    {
        leds[i].R = r;
        leds[i].G = g;
        leds[i].B = b;
    }
    npWrite(); // Atualiza o hardware
}

void npSetMatrixWithIntensity(int matriz[NP_MATRIX_HEIGHT][NP_MATRIX_WIDTH][3], float intensity)
{
    intensity = clampIntensity(intensity);

    // Loop para configurar os LEDs
    for (uint8_t linha = 0; linha < 5; linha++)
    {
        for (uint8_t coluna = 0; coluna < 5; coluna++)
        {
            // Calcula os valores RGB ajustados pela intensidade
            uint8_t r = (uint8_t)(float)(matriz[linha][coluna][0] * intensity);
            uint8_t g = (uint8_t)(float)(matriz[linha][coluna][1] * intensity);
            uint8_t b = (uint8_t)(float)(matriz[linha][coluna][2] * intensity);

            uint index = getIndex(coluna, linha);

            // Configura o LED diretamente
            leds[index].R = r;
            leds[index].G = g;
            leds[index].B = b;
        }
    }
    npWrite(); // Atualiza o hardware
}

void npAnimateFrames(int period, int num_frames,
                     int desenho[num_frames][NP_MATRIX_HEIGHT][NP_MATRIX_WIDTH][3],
                     float intensity)
{
    intensity = clampIntensity(intensity);

    for (int i = 0; i < num_frames; i++)
    {
        npSetMatrixWithIntensity(desenho[i], intensity);
        sleep_ms(period); // Aguarda o período definido entre frames
    }
}