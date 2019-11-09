#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    volatile bool pressed;
    volatile bool event_ready;
} mock_button_t;

void mock_button_init(mock_button_t *btn);
void mock_button_press(mock_button_t *btn);
void mock_button_release(mock_button_t *btn);
bool mock_button_consume_event(mock_button_t *btn);

typedef struct {
    volatile uint32_t value;
} mock_sensor_t;

void mock_sensor_init(mock_sensor_t *sensor);
void mock_sensor_set(mock_sensor_t *sensor, uint32_t value);
uint32_t mock_sensor_read(const mock_sensor_t *sensor);
