/*
 ******************************************************************************
 * @file    read_data_polling.c
 * @author  MEMS Software Solution Team
 * @brief   This file show the simplest way to get data from sensor.
 *
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

/*
 * This example was developed using the following STMicroelectronics
 * evaluation boards:
 *
 * - STEVAL_MKI109V3 + STEVAL-MKI173V1
 * - NUCLEO_F411RE + STEVAL-MKI173V1
 *
 * and STM32CubeMX tool with STM32CubeF4 MCU Package
 *
 * Used interfaces:
 *
 * STEVAL_MKI109V3    - Host side:   USB (Virtual COM)
 *                    - Sensor side: SPI(Default) / I2C(supported)
 *
 * NUCLEO_STM32F411RE - Host side: UART(COM) to USB bridge
 *                    - Sensor side: I2C(Default) / SPI(supported)
 *
 * If you need to run this example on a different hardware platform a
 * modification of the functions: `platform_write`, `platform_read`,
 * `tx_com` and 'platform_init' is required.
 *
 */

/* STMicroelectronics evaluation boards definition
 *
 * Please uncomment ONLY the evaluation boards in use.
 * If a different hardware is used please comment all
 * following target board and redefine yours.
 */
//#define STEVAL_MKI109V3
#define NUCLEO_F411RE

#if defined(STEVAL_MKI109V3)
/* MKI109V3: Define communication interface */
#define SENSOR_BUS hspi2

/* MKI109V3: Vdd and Vddio power supply values */
#define PWM_3V3 915

#elif defined(NUCLEO_F411RE)
/* NUCLEO_F411RE: Define communication interface */
#define SENSOR_BUS hi2c1

#endif

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "stm32f4xx_hal.h"
#include "lsm303ah_reg.h"
#include "gpio.h"
#include "i2c.h"
#if defined(STEVAL_MKI109V3)
#include "usbd_cdc_if.h"
#include "spi.h"
#elif defined(NUCLEO_F411RE)
#include "usart.h"
#endif

typedef union {
  int16_t i16bit[3];
  uint8_t u8bit[6];
} axis3bit16_t;

typedef struct {
  void   *hbus;
  uint8_t i2c_address;
  uint8_t cs_port;
  uint8_t cs_pin;
} sensbus_t;

/* Private macro -------------------------------------------------------------*/
#define BOOT_TIME             20 //ms
#define TX_BUF_DIM          1000

/* Private variables ---------------------------------------------------------*/
#if defined(STEVAL_MKI109V3)
static sensbus_t xl_bus  = {&SENSOR_BUS,
                            0,
                            CS_DEV_GPIO_Port,
                            CS_DEV_Pin
                           };
static sensbus_t mag_bus = {&SENSOR_BUS,
                            0,
                            CS_RF_GPIO_Port,
                            CS_RF_Pin
                           };
#elif defined(NUCLEO_F411RE)
static sensbus_t xl_bus  = {&SENSOR_BUS,
                            LSM303AH_I2C_ADD_XL,
                            0,
                            0
                           };
static sensbus_t mag_bus = {&SENSOR_BUS,
                            LSM303AH_I2C_ADD_MG,
                            0,
                            0
                           };
#endif

static axis3bit16_t data_raw_acceleration;
static axis3bit16_t data_raw_magnetic;
static uint8_t tx_buffer[TX_BUF_DIM];
static float acceleration_mg[3];
static float magnetic_mG[3];
static uint8_t whoamI, rst;

/* Extern variables ----------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
/*
 *   WARNING:
 *   Functions declare in this section are defined at the end of this file
 *   and are strictly related to the hardware platform used.
 *
 */
static int32_t platform_write(void *handle, uint8_t reg,
                              uint8_t *bufp,
                              uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len);
static void tx_com(uint8_t *tx_buffer, uint16_t len);
static void platform_delay(uint32_t ms);
static void platform_init(void);

/* Main Example --------------------------------------------------------------*/
void lsm303ah_read_data_polling(void)
{
  stmdev_ctx_t dev_ctx_xl;
  stmdev_ctx_t dev_ctx_mg;
  /* Initialize mems driver interface */
  dev_ctx_xl.write_reg = platform_write;
  dev_ctx_xl.read_reg = platform_read;
  dev_ctx_xl.handle = (void *)&xl_bus;
  dev_ctx_mg.write_reg = platform_write;
  dev_ctx_mg.read_reg = platform_read;
  dev_ctx_mg.handle = (void *)&mag_bus;
  /* Wait boot time and initialize platform specific hardware */
  platform_init();
  /* Wait sensor boot time */
  platform_delay(BOOT_TIME);
  /* Check device ID */
  whoamI = 0;
  lsm303ah_xl_device_id_get(&dev_ctx_xl, &whoamI);

  if ( whoamI != LSM303AH_ID_XL )
    while (1); /*manage here device not found */

  whoamI = 0;
  lsm303ah_mg_device_id_get(&dev_ctx_mg, &whoamI);

  if ( whoamI != LSM303AH_ID_MG )
    while (1); /*manage here device not found */

  /* Restore default configuration */
  lsm303ah_xl_reset_set(&dev_ctx_xl, PROPERTY_ENABLE);

  do {
    lsm303ah_xl_reset_get(&dev_ctx_xl, &rst);
  } while (rst);

  lsm303ah_mg_reset_set(&dev_ctx_mg, PROPERTY_ENABLE);

  do {
    lsm303ah_mg_reset_get(&dev_ctx_mg, &rst);
  } while (rst);

  /* Enable Block Data Update */
  lsm303ah_xl_block_data_update_set(&dev_ctx_xl, PROPERTY_ENABLE);
  lsm303ah_mg_block_data_update_set(&dev_ctx_mg, PROPERTY_ENABLE);
  /* Set full scale */
  lsm303ah_xl_full_scale_set(&dev_ctx_xl, LSM303AH_XL_2g);
  /* Configure filtering chain */
  /* Accelerometer - High Pass / Slope path */
  //lsm303ah_xl_hp_path_set(&dev_ctx_xl, LSM303AH_HP_ON_OUTPUTS);
  /* Set / Reset magnetic sensor mode */
  lsm303ah_mg_set_rst_mode_set(&dev_ctx_mg,
                               LSM303AH_MG_SENS_OFF_CANC_EVERY_ODR);
  /* Enable temperature compensation on mag sensor */
  lsm303ah_mg_offset_temp_comp_set(&dev_ctx_mg, PROPERTY_ENABLE);
  /* Set Output Data Rate */
  lsm303ah_xl_data_rate_set(&dev_ctx_xl, LSM303AH_XL_ODR_100Hz_LP);
  lsm303ah_mg_data_rate_set(&dev_ctx_mg, LSM303AH_MG_ODR_10Hz);
  /* Set magnetometer in continuous mode */
  lsm303ah_mg_operating_mode_set(&dev_ctx_mg,
                                 LSM303AH_MG_CONTINUOUS_MODE);

  /* Read samples in polling mode (no int) */
  while (1) {
    /* Read output only if new value is available */
    lsm303ah_reg_t reg;
    lsm303ah_xl_status_reg_get(&dev_ctx_xl, &reg.status_a);

    if (reg.status_a.drdy) {
      /* Read acceleration data */
      memset(data_raw_acceleration.u8bit, 0x00, 3 * sizeof(int16_t));
      lsm303ah_acceleration_raw_get(&dev_ctx_xl,
                                    data_raw_acceleration.u8bit);
      acceleration_mg[0] = lsm303ah_from_fs2g_to_mg(
                             data_raw_acceleration.i16bit[0]);
      acceleration_mg[1] = lsm303ah_from_fs2g_to_mg(
                             data_raw_acceleration.i16bit[1]);
      acceleration_mg[2] = lsm303ah_from_fs2g_to_mg(
                             data_raw_acceleration.i16bit[2]);
      sprintf((char *)tx_buffer,
              "Acceleration [mg]:%4.2f\t%4.2f\t%4.2f\r\n",
              acceleration_mg[0], acceleration_mg[1], acceleration_mg[2]);
      tx_com( tx_buffer, strlen( (char const *)tx_buffer ) );
    }

    lsm303ah_mg_status_get(&dev_ctx_mg, &reg.status_reg_m);

    if (reg.status_reg_m.zyxda) {
      /* Read magnetic field data */
      memset(data_raw_magnetic.u8bit, 0x00, 3 * sizeof(int16_t));
      lsm303ah_magnetic_raw_get(&dev_ctx_mg, data_raw_magnetic.u8bit);
      magnetic_mG[0] = lsm303ah_from_lsb_to_mgauss(
                         data_raw_magnetic.i16bit[0]);
      magnetic_mG[1] = lsm303ah_from_lsb_to_mgauss(
                         data_raw_magnetic.i16bit[1]);
      magnetic_mG[2] = lsm303ah_from_lsb_to_mgauss(
                         data_raw_magnetic.i16bit[2]);
      sprintf((char *)tx_buffer,
              "Magnetic field [mG]:%4.2f\t%4.2f\t%4.2f\r\n",
              magnetic_mG[0], magnetic_mG[1], magnetic_mG[2]);
      tx_com( tx_buffer, strlen( (char const *)tx_buffer ) );
    }
  }
}

/*
 * @brief  Write generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to write
 * @param  bufp      pointer to data to write in register reg
 * @param  len       number of consecutive register to write
 *
 */
static int32_t platform_write(void *handle, uint8_t reg,
                              uint8_t *bufp,
                              uint16_t len)
{
  sensbus_t *sensbus = (sensbus_t *)handle;

  if (sensbus->hbus == &hi2c1) {
    /* Write multiple command */
    reg |= 0x80;
    HAL_I2C_Mem_Write(sensbus->hbus, sensbus->i2c_address, reg,
                      I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
  }

#if defined(STEVAL_MKI109V3)

  else if (sensbus->hbus == &hspi2) {
    /* Write multiple command */
    reg |= 0x40;
    HAL_GPIO_WritePin(sensbus->cs_port, sensbus->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(sensbus->hbus, &reg, 1, 1000);
    HAL_SPI_Transmit(sensbus->hbus, bufp, len, 1000);
    HAL_GPIO_WritePin(sensbus->cs_port, sensbus->cs_pin, GPIO_PIN_SET);
  }

#endif
  return 0;
}

/*
 * @brief  Read generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to read
 * @param  bufp      pointer to buffer that store the data read
 * @param  len       number of consecutive register to read
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp,
                             uint16_t len)
{
  sensbus_t *sensbus = (sensbus_t *)handle;

  if (sensbus->hbus == &hi2c1) {
    /* Read multiple command */
    reg |= 0x80;
    HAL_I2C_Mem_Read(sensbus->hbus, sensbus->i2c_address, reg,
                     I2C_MEMADD_SIZE_8BIT, bufp, len, 1000);
  }

#if defined(STEVAL_MKI109V3)

  else if (sensbus->hbus == &hspi2) {
    /* Read multiple command */
    reg |= 0xC0;
    HAL_GPIO_WritePin(sensbus->cs_port, sensbus->cs_pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(sensbus->hbus, &reg, 1, 1000);
    HAL_SPI_Receive(sensbus->hbus, bufp, len, 1000);
    HAL_GPIO_WritePin(sensbus->cs_port, sensbus->cs_pin, GPIO_PIN_SET);
  }

#endif
  return 0;
}

/*
 * @brief  Send buffer to console (platform dependent)
 *
 * @param  tx_buffer     buffer to transmit
 * @param  len           number of byte to send
 *
 */
static void tx_com(uint8_t *tx_buffer, uint16_t len)
{
#if defined(STEVAL_MKI109V3)
  CDC_Transmit_FS(tx_buffer, len);
#elif defined(NUCLEO_F411RE)
  HAL_UART_Transmit(&huart2, tx_buffer, len, 1000);
#endif
}

/*
 * @brief  platform specific delay (platform dependent)
 */
static void platform_delay(uint32_t ms)
{
  HAL_Delay(ms);
}

/*
 * @brief  platform specific initialization (platform dependent)
 */
static void platform_init(void)
{
#if defined(STEVAL_MKI109V3)
  TIM3->CCR1 = PWM_3V3;
  TIM3->CCR2 = PWM_3V3;
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_Delay(1000);
#endif
}
