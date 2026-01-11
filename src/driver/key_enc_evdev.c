#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <errno.h>

#include "utils/log.h"
#include "lvgl.h"
#include "driver/key_enc_evdev.h"


static int key_enc_evdev_process_key(uint16_t code)
{
    switch(code) {
        case KEY_1:
            return LV_KEY_LEFT;
        case KEY_2:
            return LV_KEY_RIGHT;
        case KEY_3:
            return LV_KEY_ENTER;
        case KEY_4:
            return LV_KEY_ESC;
        case KEY_0:
            return LV_KEY_END;
        default:
            return 0;
    }
}

void print_lv_key_str(int key){
    switch(key){
        case LV_KEY_LEFT:
            log_debug("LV_KEY_LEFT");
            break;
        case LV_KEY_RIGHT:
            log_debug("LV_KEY_RIGHT");
            break;
        case LV_KEY_ENTER:
            log_debug("LV_KEY_ENTER");
            break;
        case LV_KEY_ESC:
            log_debug("LV_KEY_ESC");
            break;
        case LV_KEY_END:
            log_debug("LV_KEY_END");
            break;
        default:
            log_debug("unknown key: %d", key);
    }
}

// 官方文档中说只需要在key_pressed的时候回传data->key
// 实际上松开的事件也需要回传，否则默认的事件是“放开了enter按钮”，导致误触发选择。
// 此外 evdev只会在按钮按下/松开的时候产生数据（被read），因此需要缓存数据。
static void key_enc_evdev_read_cb(lv_indev_t * indev, lv_indev_data_t * data){

    static int last_key = 0;
    static int last_state = LV_INDEV_STATE_RELEASED;

    key_enc_evdev_t * key_enc_evdev = (key_enc_evdev_t *)lv_indev_get_driver_data(indev);
    struct input_event in = { 0 };
    while(1){
        int bytes_read = read(key_enc_evdev->evdev_fd, &in, sizeof(in));
        // log_debug("key_enc_evdev_read_cb: bytes_read = %d, type = %d, code = %d, value = %d", bytes_read, in.type, in.code, in.value);
        if(bytes_read <= 0){
            goto return_with_last_value;
        }
        if(in.type != EV_KEY){
            continue;
        }

        if(in.value == 1) {
            /* Get the last pressed or released key
            * use LV_KEY_ENTER for encoder press */
            data->key = key_enc_evdev_process_key(in.code);
            data->state = LV_INDEV_STATE_PRESSED;
            // data->continue_reading = true;
            key_enc_evdev->input_cb(data->key);
            last_key = data->key;
            last_state = data->state;
            // print_lv_key_str(data->key);
            // log_debug("key_enc_evdev_read_cb: key = %d pressed", data->key);
            return;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
            data->key = last_key;
            last_state = data->state;
            // print_lv_key_str(data->key);
            // log_debug("key_enc_evdev_read_cb:  release", data->key);
            return;
        }
    }

return_with_last_value:
    data->key = last_key;
    data->state = last_state;
    return;
}

lv_indev_t * key_enc_evdev_init(key_enc_evdev_t * key_enc_evdev){
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev, key_enc_evdev_read_cb);
    key_enc_evdev->indev = indev;
    lv_indev_set_driver_data(indev, key_enc_evdev);

    key_enc_evdev->evdev_fd = open(key_enc_evdev->dev_path, O_RDONLY);
    if(key_enc_evdev->evdev_fd < 0){
        log_error("Failed to open evdev file: %s", key_enc_evdev->dev_path);
        return NULL;
    }

    if(fcntl(key_enc_evdev->evdev_fd, F_SETFL, O_NONBLOCK) < 0) {
        log_error("fcntl failed: %d", errno);
        return NULL;
    }

    return indev;
}

void key_enc_evdev_destroy(key_enc_evdev_t * key_enc_evdev){
    close(key_enc_evdev->evdev_fd);
    lv_indev_delete(key_enc_evdev->indev);
}