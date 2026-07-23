#include "codex_controls.h"

#include <rtthread.h>
#include "../middleware/event_bus.h"
#include "../device/encoder_controller.h"
#include "../device/hid_device.h"

static bool g_initialized;

static int codex_controls_encoder_handler(const event_t *event, void *user_data)
{
    int32_t delta;
    int steps;
    (void)user_data;
    if (!event || event->type != EVENT_ENCODER_ROTATED || !hid_device_ready()) return 0;

    delta = event->data.encoder.delta;
    steps = delta > 0 ? delta : -delta;
    if (steps > 3) steps = 3;
    while (steps-- > 0) {
        hid_consumer_click(delta > 0 ? CC_VOL_UP : CC_VOL_DOWN);
        rt_thread_mdelay(10);
    }
    return 0;
}

int codex_controls_init(void)
{
    if (g_initialized) return 0;
    event_bus_subscribe(EVENT_ENCODER_ROTATED, codex_controls_encoder_handler,
                        RT_NULL, EVENT_PRIORITY_HIGH);
    if (encoder_controller_is_ready()) {
        encoder_controller_reset_count();
        encoder_controller_set_mode(ENCODER_MODE_VOLUME);
        encoder_controller_set_sensitivity(1);
        encoder_controller_start_polling();
    }
    g_initialized = true;
    return 0;
}

void codex_controls_deinit(void)
{
    if (!g_initialized) return;
    event_bus_unsubscribe(EVENT_ENCODER_ROTATED, codex_controls_encoder_handler);
    encoder_controller_stop_polling();
    g_initialized = false;
}
