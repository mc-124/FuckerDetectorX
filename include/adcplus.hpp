#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include <Arduino.h>

inline float read_adc_voltage(gpio_num_t pin) {
    static adc_oneshot_unit_handle_t adc_handle = NULL;
    static adc_cali_handle_t cali_handle = NULL;
    static bool initialized = false;

    if (!initialized) {
        adc_oneshot_unit_init_cfg_t unit_cfg = {
            .unit_id = ADC_UNIT_1,
        };
        if (adc_oneshot_new_unit(&unit_cfg, &adc_handle) != ESP_OK) {
            return -1.0f;
        }

        adc_cali_curve_fitting_config_t cali_cfg = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
        if (ret != ESP_OK) {
            log_e("cali error: %d", static_cast<int>(ret));
            cali_handle = NULL;
        }
        initialized = true;
    }

    /// @warning 这段代码只适配了ESP32-C3
    adc_channel_t channel;
    switch (pin) {
        case GPIO_NUM_0:channel=ADC_CHANNEL_0;break;
        case GPIO_NUM_1:channel=ADC_CHANNEL_1;break;
        case GPIO_NUM_2:channel=ADC_CHANNEL_2;break;
        case GPIO_NUM_3:channel=ADC_CHANNEL_3;break;
        case GPIO_NUM_4:channel=ADC_CHANNEL_4;break;
        case GPIO_NUM_5:channel=ADC_CHANNEL_5;break;
        default:
            return -1.0f;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_oneshot_config_channel(adc_handle, channel, &chan_cfg) != ESP_OK) {
        return -1.0f;
    }

    int raw;
    if (adc_oneshot_read(adc_handle, channel, &raw) != ESP_OK) {
        return -1.0f;
    }

    int voltage_mv;
    if (cali_handle != NULL) {
        if (adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv) != ESP_OK) {
            return -1.0f;
        }
    } else {
        voltage_mv = raw * 3300 / 4095;
    }
    return (float)voltage_mv / 1000.0f;
}