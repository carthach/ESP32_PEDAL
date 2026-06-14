#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#ifdef WOKWI
#include <Wire.h>
#include <Adafruit_FT6206.h> // Touch library
Adafruit_FT6206 ctp = Adafruit_FT6206();
#endif

TFT_eSPI tft = TFT_eSPI();

/* Display flushing: write the internal buffer to the display */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

/* Tick source for LVGL */
static uint32_t my_tick(void) {
    return millis();
}

#ifdef WOKWI
void touch_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    if(ctp.touched()) {        
        TS_Point point = ctp.getPoint();        
        data->point.x = point.y;
        data->point.y = map(point.x, 240, 0, 0, tft.height());

        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#else
void touch_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    uint16_t t_x, t_y;
    bool pressed = tft.getTouch(&t_x, &t_y);
    if(pressed) {                
        data->point.x = t_x;
        data->point.y = t_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
#endif

static void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target_obj(e);
    if(code == LV_EVENT_CLICKED) {
        
        static uint8_t cnt = 0;
        cnt++;
        /*Get the first child of the button which is the label and change its text*/
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Button: %d", cnt);
    }
}

static lv_obj_t * label;

static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target_obj(e);
    /*Refresh the text*/
    lv_label_set_text_fmt(label, "%" LV_PRId32, lv_slider_get_value(slider));
    lv_obj_align_to(label, slider, LV_ALIGN_OUT_TOP_MID, 0, -15);    /*Align top of the slider*/
}

void setup() {    
    Serial.begin(115200);
    
    lv_init();
    lv_tick_set_cb(my_tick);

    tft.begin();
    tft.setRotation(1);  // Match the landscape orientation of the display

      // Calibrate the touch screen and retrieve the scaling factors
#ifdef WOKWI
  Wire.begin(46, 3); 
  ctp.begin();
#else
//   touch_calibrate();
#endif

    /* Create a display object */
    lv_display_t *disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // This needs to come after the display is initialized
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read);
    
    /* Initialize buffers */
    static uint8_t buf1[320 * 40]; 
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_obj_t *btn = lv_btn_create(lv_screen_active());
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 40);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Click Me");
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    
    /*Create a slider in the center of the display*/
    lv_obj_t * slider = lv_slider_create(lv_screen_active());
    lv_obj_set_width(slider, 200);                          /*Set the width*/
    lv_obj_center(slider);                                  /*Align to the center of the parent (screen)*/
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);     /*Assign an event function*/
    
    /*Create a label above the slider*/
    label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "0");
    lv_obj_align_to(label, slider, LV_ALIGN_OUT_TOP_MID, 0, -15);    /*Align top of the slider*/
}

void loop() {
    lv_timer_handler(); /* Update UI */
    delay(5);    
}
