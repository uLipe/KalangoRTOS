#include "mock_peripheral.h"

void mock_button_init(mock_button_t *btn)
{
    btn->pressed = false;
    btn->event_ready = false;
}

void mock_button_press(mock_button_t *btn)
{
    btn->pressed = true;
    btn->event_ready = true;
}

void mock_button_release(mock_button_t *btn)
{
    btn->pressed = false;
    btn->event_ready = true;
}

bool mock_button_consume_event(mock_button_t *btn)
{
    if (!btn->event_ready) {
        return false;
    }

    btn->event_ready = false;
    return true;
}

void mock_sensor_init(mock_sensor_t *sensor)
{
    sensor->value = 0;
}

void mock_sensor_set(mock_sensor_t *sensor, uint32_t value)
{
    sensor->value = value;
}

uint32_t mock_sensor_read(const mock_sensor_t *sensor)
{
    return sensor->value;
}
