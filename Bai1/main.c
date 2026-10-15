/*
 * BAI 01 - STM32F103C8T6 + FreeRTOS + HAL
 * 3 PWM LEDs @ 1 kHz, 3 buttons, 3 independent LED tasks,
 * 1 button task, 3 FreeRTOS queues.
 *
 * Hardware: 8 MHz HSE (Blue Pill) -> 72 MHz SYSCLK.
 * PWM: TIM3_CH1=PA6, TIM3_CH2=PA7, TIM3_CH3=PB0.
 * Buttons: PB12=SELECT, PB13=UP, PB14=DOWN (to GND, pull-up).
 *
 * Use with a CubeIDE/CubeMX-generated STM32F103C8Tx project:
 * - Enable FreeRTOS (CMSIS_V1, dynamic allocation, heap_4).
 * - Leave TIM3 and GPIO pins unconfigured in CubeMX: this file configures them.
 * - Replace Core/Src/main.c with this file.
 * - The generated SysTick/FreeRTOS handlers must remain in the project.
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>

#define LED_COUNT             3U
#define PWM_PERIOD_COUNTS     1000U
#define LED_BRIGHTNESS_STEP   10U
#define BUTTON_POLL_MS        10U
#define DEBOUNCE_SAMPLES      3U

static TIM_HandleTypeDef htim3;
static QueueHandle_t led_queues[LED_COUNT] = {NULL, NULL, NULL};
static const uint32_t led_channels[LED_COUNT] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3
};
static const uint16_t button_pins[LED_COUNT] = {
    GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14
};

typedef struct {
    uint8_t stable;
    uint8_t last_sample;
    uint8_t samples;
} ButtonDebounce;

static void SystemClock_Config(void);
static void PWM_GPIO_Init(void);
static void TIM3_PWM_Init(void);
static void Button_Task(void *argument);
static void LED_Task(void *argument);
void Error_Handler(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    PWM_GPIO_Init();
    TIM3_PWM_Init();

    for (uint32_t i = 0; i < LED_COUNT; i++) {
        led_queues[i] = xQueueCreate(1U, sizeof(uint8_t));
        if (led_queues[i] == NULL) {
            Error_Handler();
        }
    }

    if (xTaskCreate(LED_Task, "LED1", 128U,
                    (void *)&led_channels[0], tskIDLE_PRIORITY + 2U, NULL) != pdPASS ||
        xTaskCreate(LED_Task, "LED2", 128U,
                    (void *)&led_channels[1], tskIDLE_PRIORITY + 2U, NULL) != pdPASS ||
        xTaskCreate(LED_Task, "LED3", 128U,
                    (void *)&led_channels[2], tskIDLE_PRIORITY + 2U, NULL) != pdPASS ||
        xTaskCreate(Button_Task, "Buttons", 192U,
                    NULL, tskIDLE_PRIORITY + 1U, NULL) != pdPASS) {
        Error_Handler();
    }

    /* Initially all LEDs are off. Send initial states to each queue. */
    uint8_t initial_brightness = 0U;
    for (uint32_t i = 0; i < LED_COUNT; i++) {
        xQueueOverwrite(led_queues[i], &initial_brightness);
    }

    vTaskStartScheduler();
    Error_Handler(); /* No return is expected. */
}

static void LED_Task(void *argument)
{
    uint32_t channel = *(const uint32_t *)argument;
    uint32_t index;
    uint8_t brightness = 0U;

    if (channel == TIM_CHANNEL_1) {
        index = 0U;
    } else if (channel == TIM_CHANNEL_2) {
        index = 1U;
    } else {
        index = 2U;
    }

    for (;;) {
        /* Blocks until the button task sends a fresh brightness value. */
        if (xQueueReceive(led_queues[index], &brightness, portMAX_DELAY) == pdTRUE) {
            /* ARR=999, so 0..1000 gives 0..100% duty cycle. */
            uint32_t compare = ((uint32_t)brightness * PWM_PERIOD_COUNTS) / 100U;
            __HAL_TIM_SET_COMPARE(&htim3, channel, compare);
        }
    }
}

static void Button_Task(void *argument)
{
    (void)argument;
    uint8_t brightness[LED_COUNT] = {0U, 0U, 0U};
    uint8_t selected_led = 0U; /* LED1 initially selected. */
    ButtonDebounce buttons[LED_COUNT] = {
        {1U, 1U, 0U}, {1U, 1U, 0U}, {1U, 1U, 0U}
    };

    for (;;) {
        for (uint32_t i = 0; i < LED_COUNT; i++) {
            uint8_t sample = (HAL_GPIO_ReadPin(GPIOB, button_pins[i]) == GPIO_PIN_SET)
                                 ? 1U : 0U;

            if (sample == buttons[i].last_sample) {
                if (buttons[i].samples < DEBOUNCE_SAMPLES) {
                    buttons[i].samples++;
                }
            } else {
                buttons[i].last_sample = sample;
                buttons[i].samples = 1U;
            }

            if (buttons[i].samples >= DEBOUNCE_SAMPLES &&
                sample != buttons[i].stable) {
                buttons[i].stable = sample;
                if (sample == 0U) { /* One event on each debounced press. */
                    if (i == 0U) {
                        selected_led = (uint8_t)((selected_led + 1U) % LED_COUNT);
                    } else if (i == 1U) {
                        if (brightness[selected_led] <= 100U - LED_BRIGHTNESS_STEP) {
                            brightness[selected_led] += LED_BRIGHTNESS_STEP;
                        } else {
                            brightness[selected_led] = 100U;
                        }
                    } else {
                        if (brightness[selected_led] >= LED_BRIGHTNESS_STEP) {
                            brightness[selected_led] -= LED_BRIGHTNESS_STEP;
                        } else {
                            brightness[selected_led] = 0U;
                        }
                    }

                    /* Queue length = 1, latest requested brightness wins. */
                    xQueueOverwrite(led_queues[selected_led],
                                    &brightness[selected_led]);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

static void PWM_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static void TIM3_PWM_Init(void)
{
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 71U;  /* 72 MHz / (71+1) = 1 MHz */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999U;  /* 1 MHz / (999+1) = 1 kHz */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
        Error_Handler();
    }

    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0U;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_2) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_3) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) {
        Error_Handler();
    }
}

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
    }
}

/* FreeRTOS stack overflow hook */
void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                   char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();

    while (1)
    {
        /* Stop execution if a task stack overflows */
    }
}
