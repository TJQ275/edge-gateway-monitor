/**
 * @file lv_port_indev.c
 * Touchpad (XPT2046) input port for LVGL 8.3.
 */

#include "lv_port_indev.h"
#include "touch.h"

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;
    uint16_t x, y;

    (void)indev_drv;

    if (TP_Get_Calibrated(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PR;
        last_x = x;
        last_y = y;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state   = LV_INDEV_STATE_REL;
    }
}
