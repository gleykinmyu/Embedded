#include "relay_board.h"

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "relay";

RelayBoard::Lock::Lock(SemaphoreHandle_t mu, TickType_t ticks)
    : mu_(mu), ok_(mu && xSemaphoreTake(mu, ticks) == pdTRUE) {}

RelayBoard::Lock::~Lock() {
    if (ok_) {
        xSemaphoreGive(mu_);
    }
}

bool RelayBoard::tca_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(tca_, data, sizeof(data), 100) == ESP_OK;
}

void RelayBoard::coil_on(size_t ch) {
    shadow_ |= static_cast<uint8_t>(1u << PIN_RELAY[ch]);
    tca_write(TCA_REG_OUTPUT, shadow_);
}

void RelayBoard::coil_off(size_t ch) {
    shadow_ &= static_cast<uint8_t>(~(1u << PIN_RELAY[ch]));
    tca_write(TCA_REG_OUTPUT, shadow_);
}

void RelayBoard::coils_off() {
    shadow_ = 0x00;
    tca_write(TCA_REG_OUTPUT, shadow_);
}

bool RelayBoard::coil_is_on(size_t ch) const {
    return (shadow_ & static_cast<uint8_t>(1u << PIN_RELAY[ch])) != 0;
}

void RelayBoard::chase_kick() {
    if (chase_task_) {
        xTaskNotifyGive(chase_task_);
    }
}

void RelayBoard::chase_task(void *arg) {
    static_cast<RelayBoard *>(arg)->chase_loop();
}

void RelayBoard::chase_loop() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RELAY_STEP_MS));
        Lock g(mu_, portMAX_DELAY);
        if (!g || !chase_running_) {
            continue;
        }
        size_t ch = chase_ch_;
        chase_ch_ = (chase_ch_ + 1) % RELAY_COUNT;
        coils_off();
        coil_on(ch);
    }
}

void RelayBoard::begin() {
    mu_ = xSemaphoreCreateMutex();

    gpio_config_t buzzer = {};
    buzzer.pin_bit_mask = 1ULL << PIN_BUZZER;
    buzzer.mode = GPIO_MODE_OUTPUT;
    gpio_config(&buzzer);
    gpio_set_level(static_cast<gpio_num_t>(PIN_BUZZER), 0);

    uint64_t mask = 0;
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        mask |= 1ULL << PIN_DI[i];
    }
    gpio_config_t di = {};
    di.pin_bit_mask = mask;
    di.mode = GPIO_MODE_INPUT;
    di.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&di);

    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = static_cast<gpio_num_t>(PIN_I2C_SDA);
    bus_cfg.scl_io_num = static_cast<gpio_num_t>(PIN_I2C_SCL);
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus = nullptr;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));
    if (i2c_master_probe(bus, TCA9554_ADDR, 100) != ESP_OK) {
        ESP_LOGE(TAG, "TCA9554 not found at 0x%02X", TCA9554_ADDR);
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = TCA9554_ADDR;
    dev_cfg.scl_speed_hz = 100000;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &tca_));
    tca_write(TCA_REG_CONFIG, 0x00);
    coils_off();

    xTaskCreate(chase_task, "chase", 2048, this, 5, &chase_task_);
}

bool RelayBoard::set(size_t ch, bool on) {
    if (ch >= RELAY_COUNT) {
        return false;
    }
    Lock g(mu_);
    if (!g) {
        return false;
    }
    chase_running_ = false;
    if (on) {
        coil_on(ch);
    } else {
        coil_off(ch);
    }
    return true;
}

bool RelayBoard::is_on(size_t ch) {
    Lock g(mu_, pdMS_TO_TICKS(50));
    return g && coil_is_on(ch);
}

bool RelayBoard::all_on() {
    Lock g(mu_);
    if (!g) {
        return false;
    }
    chase_running_ = false;
    for (size_t ch = 0; ch < RELAY_COUNT; ++ch) {
        coil_on(ch);
    }
    return true;
}

bool RelayBoard::all_off() {
    Lock g(mu_);
    if (!g) {
        return false;
    }
    chase_running_ = false;
    coils_off();
    return true;
}

bool RelayBoard::chase_start() {
    Lock g(mu_);
    if (!g) {
        return false;
    }
    chase_running_ = true;
    chase_kick();
    return true;
}

bool RelayBoard::chase_stop() {
    Lock g(mu_);
    if (!g) {
        return false;
    }
    chase_running_ = false;
    coils_off();
    return true;
}

RelayBoard::Snapshot RelayBoard::snapshot() {
    Snapshot s = {};
    Lock g(mu_);
    if (!g) {
        s.busy = true;
        return s;
    }
    s.chase = chase_running_;
    for (size_t ch = 0; ch < RELAY_COUNT; ++ch) {
        s.on[ch] = coil_is_on(ch);
    }
    return s;
}

void RelayBoard::print() {
    const Snapshot s = snapshot();
    if (s.busy) {
        printf("relays busy\n");
        return;
    }
    printf("relays");
    for (size_t ch = 0; ch < RELAY_COUNT; ++ch) {
        printf(" CH%u=%s", static_cast<unsigned>(ch + 1), s.on[ch] ? "ON" : "OFF");
    }
    printf(" chase=%s\n", s.chase ? "run" : "stop");
}
