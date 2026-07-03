#include <lvgl.h>
#include <TFT_eSPI.h>

#define ESP32_HOST_MIDI_NO_USB_HOST
#include <ESP32_Host_MIDI.h>

#ifdef WOKWI
#include <WiFi.h>
#include <AppleMIDI.h>
#include "RTPMIDIConnection.h"
RTPMIDIConnection rtpMIDI;
#else
#include "USBDeviceConnection.h"
USBDeviceConnection usbMIDI("ESP32 MIDI");
#endif

lv_obj_t *sliders[4];
lv_obj_t * btns[4];

#define N_INPUTS 3

int pot_pins[] = {4, 5, 6};
int switch_pins[] = {1, 2, 42};

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
        data->point.y = map(t_y, 0, 240, tft.height(), 0);
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
        for(int i=0; i<4; i++) {
            if(btn == btns[i]) {
                bool isChecked = lv_obj_has_state(btn, LV_STATE_CHECKED);
                midiHandler.sendControlChange(i+1, 2, isChecked ? 127 : 0);                
            }
        }
    }
}

static lv_obj_t * label;

static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target_obj(e);

    for(int i=0; i<4; i++) {
        if(slider == sliders[i]) {
            int32_t value = lv_slider_get_value(slider);        
            midiHandler.sendControlChange(i+1, 1, value);
        }
    }

    
}

void setup() {           
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

    lv_obj_t * screen = lv_screen_active();
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(screen, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(screen, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_track_place(screen, LV_FLEX_ALIGN_CENTER, 0);

    /* Main grid container with fixed descriptors */
    lv_obj_t * container = lv_obj_create(screen);
    int32_t cell_width = 65;
    static const int32_t container_style_grid_column_dsc_array_0[] = {cell_width, cell_width, cell_width, cell_width, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_style_grid_column_dsc_array(container, container_style_grid_column_dsc_array_0, 0);
    static const int32_t container_style_grid_row_dsc_array_1[] = {160, 50, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_style_grid_row_dsc_array(container, container_style_grid_row_dsc_array_1, 0);
    lv_obj_set_style_layout(container, LV_LAYOUT_GRID, 0);
    lv_obj_set_size(container, 320, 240);
    
    for(int i=0; i<4; i++) {
        sliders[i] = lv_slider_create(container);
        lv_slider_set_orientation(sliders[i], LV_SLIDER_ORIENTATION_VERTICAL);    
        lv_obj_set_size(sliders[i], 30, 140);    
        lv_obj_set_style_grid_cell_x_align(sliders[i], LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_column_pos(sliders[i], i, 0);
        lv_obj_set_style_grid_cell_y_align(sliders[i], LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_row_pos(sliders[i], 0, 0);
        lv_obj_add_event_cb(sliders[i], slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_slider_set_range(sliders[i], 0, 127);
        
        btns[i] = lv_btn_create(container);
        lv_obj_set_style_grid_cell_x_align(btns[i], LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_column_pos(btns[i], i, 0);
        lv_obj_set_style_grid_cell_y_align(btns[i], LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_row_pos(btns[i], 1, 0);
        lv_obj_add_event_cb(btns[i], btn_event_cb, LV_EVENT_CLICKED, NULL);
        
    
        lv_obj_set_flag(btns[i], LV_OBJ_FLAG_CHECKABLE, true);
        lv_obj_set_state(btns[i], LV_STATE_CHECKED, true);
        lv_obj_t * label = lv_label_create(btns[i]);
        lv_obj_set_align(label, LV_ALIGN_CENTER);
        lv_label_set_text(label, itoa(i, new char[3], 10));
    }

#ifdef WOKWI
    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    midiHandler.addTransport(&rtpMIDI);
    rtpMIDI.begin();

    // Needed for Wokwi to connect to the free Wokwi gateway (host.wokwi.internal) for AppleMIDI.
    // I don't know how or why but thanks Claude    
    IPAddress ip;
    if (WiFi.hostByName("host.wokwi.internal", ip))
        _rtpMidiInternal::Applemidi.sendInvite(ip, 5004);
#else
    midiHandler.addTransport(&usbMIDI);
    usbMIDI.begin();    
#endif

    midiHandler.begin();

    for(int i=0; i<N_INPUTS; i++) {
        pinMode(pot_pins[i], INPUT);
        pinMode(switch_pins[i], INPUT_PULLUP);
    }

    // Serial.begin(115200);
    // Serial.println("USB Serial connection established successfully!");
}

void loop() {
    midiHandler.task(); /* Service transports — required for AppleMIDI session handshake/keepalive */
    lv_timer_handler(); /* Update UI */
    delay(20);
    
    for(int i=0; i<N_INPUTS; i++) {
        int raw_adc = analogRead(pot_pins[i]);
        int slider_value = map(raw_adc, 0, 4095, 0, 127);
        lv_slider_set_value(sliders[i], slider_value, LV_ANIM_ON);

        int switch_state = digitalRead(switch_pins[i]);                
        lv_obj_set_state(btns[i], LV_STATE_CHECKED, switch_state ? false : true);
    }

    // int note = random(21, 108);    
    // midiHandler.sendNoteOn(1, note, 100);
    // delay(1000);    
    // midiHandler.sendNoteOff(1, note, 0);
}
