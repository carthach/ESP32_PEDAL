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

#define N_TRACKS 4

// --- Bright neon LED colour scheme -----------------------------------------
#define COL_BG        0x03040A  // near-black, lets neon pop on the ILI9341
#define COL_PANEL     0x0B0F1C  // slightly lifted panel
#define COL_TRACK     0x1E2A44  // unlit slider channel (brighter, more visible)
#define COL_LED       0x00FFF0  // full-bright neon cyan - lit indicator
#define COL_LED_KNOB  0xCCFFFF  // glowing icy-white knob
#define COL_DIM_TRACK 0x161C2E  // locked slider channel
#define COL_DIM_LED   0x2E5A66  // locked indicator (dim teal)
#define COL_DIM_KNOB  0x477C8C  // locked knob (steel teal)
#define COL_ACCENT    0xFF2BD1  // hot neon magenta selection highlight
#define COL_TEXT      0xEBFFFF  // bright near-white cyan text

// Soft-takeover state for a single track's slider.
//   PICKUP - slider is locked (greyed out); the pot must reach its value first
//   LIVE   - the pot has caught up and now drives the slider
enum SyncState {PICKUP, LIVE};

typedef struct{
    lv_obj_t *slider;
    lv_obj_t *btn;
    SyncState state;
    int last_cc;    // last CC value sent, so we only transmit on a real change
} track;

track tracks[N_TRACKS];

#define N_INPUTS 3

int pot_pins[] = {4, 5, 6};
int switch_pins[] = {1, 2, 42};

int left_state, right_state;
int selected_column = 1;

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

static lv_obj_t * label;

lv_obj_t * container;
lv_obj_t * col_highlight;

static void highlight_column(int col)    
{
    /* 1. Remove any existing style from the highlight object */    
    lv_obj_remove_style_all(col_highlight); 

    /* 2. Position it to cover Column index 1, spanning all 3 rows */
    lv_obj_set_grid_cell(col_highlight, 
                        LV_GRID_ALIGN_STRETCH, col, 1,  /* Align, Col Index (1), Col Span (1) */
                        LV_GRID_ALIGN_STRETCH, 0, 2); /* Align, Row Index (0), Row Span (3) */

    /* 3. Define and apply the border style */
    static lv_style_t col_style;
    lv_style_init(&col_style);
    
    // Set border color, width, and full opacity
    lv_style_set_border_color(&col_style, lv_color_hex(COL_ACCENT));
    lv_style_set_border_width(&col_style, 3); // Adjust thickness (in pixels) as needed
    lv_style_set_border_opa(&col_style, LV_OPA_COVER);

    // Apply the style to the column highlight object
    lv_obj_add_style(col_highlight, &col_style, LV_PART_MAIN);
}

static void disable_slider(lv_obj_t* slider)
{
    lv_obj_set_state(slider, LV_STATE_DISABLED, true);

    // Dim the Main Track background
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_DIM_TRACK), LV_PART_MAIN | LV_STATE_DISABLED);

    // Dim the Indicator (the filled progression bar)
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_DIM_LED), LV_PART_INDICATOR | LV_STATE_DISABLED);

    // Dim the Knob (the circular handle)
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_DIM_KNOB), LV_PART_KNOB | LV_STATE_DISABLED);
}

static void enable_slider(lv_obj_t* slider)
{
    lv_obj_set_state(slider, LV_STATE_DISABLED, false);

    // Restore the Main Track background color
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_TRACK), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Restore the Indicator color
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_LED), LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // Restore the Knob color
    lv_obj_set_style_bg_color(slider, lv_color_hex(COL_LED_KNOB), LV_PART_KNOB | LV_STATE_DEFAULT);
}

// A single pot drives whichever column is selected. To avoid the slider jumping
// when you switch columns (the pot rarely matches the new track's value), the
// slider is locked until the pot moves to within PICKUP_THRESHOLD of it.
static const int PICKUP_THRESHOLD = 2;

// Lock the slider and wait for the pot to catch up before it takes over.
static void arm_pickup(track &t)
{
    disable_slider(t.slider);
    t.state = PICKUP;
}

// Drive the selected track's slider from the pot, honouring soft takeover.
static void sync_track_to_pot(track &t, int pot_value)
{
    if(t.state == PICKUP) {
        int slider_value = lv_slider_get_value(t.slider);
        if(abs(pot_value - slider_value) > PICKUP_THRESHOLD)
            return;                     // pot hasn't caught up yet - stay locked
        enable_slider(t.slider);
        t.state = LIVE;
    }

    lv_slider_set_value(t.slider, pot_value, LV_ANIM_OFF);

    // Only transmit when the value actually changed. ADC noise flickers the
    // reading by a step or two while the pot is still; without this the display
    // jitters and MIDI is flooded with redundant CCs.
    if(pot_value != t.last_cc) {
        midiHandler.sendControlChange(1, selected_column + 1, pot_value);
        t.last_cc = pot_value;
    }
}

// Move the selection, clamped to valid columns, and re-arm pickup on the
// newly selected track so its slider waits for the pot again.
static void select_column(int col)
{
    if(col < 0) col = 0;
    if(col > N_TRACKS - 1) col = N_TRACKS - 1;
    if(col == selected_column) return;

    selected_column = col;
    arm_pickup(tracks[col]);
    highlight_column(col);
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
    lv_obj_set_style_bg_color(screen, lv_color_hex(COL_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_main_place(screen, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(screen, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_track_place(screen, LV_FLEX_ALIGN_CENTER, 0);

    /* Main grid container with fixed descriptors */
    container = lv_obj_create(screen);
    int32_t cell_width = 65;
    static const int32_t container_style_grid_column_dsc_array_0[] = {cell_width, cell_width, cell_width, cell_width, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_style_grid_column_dsc_array(container, container_style_grid_column_dsc_array_0, 0);
    static const int32_t container_style_grid_row_dsc_array_1[] = {160, 50, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_style_grid_row_dsc_array(container, container_style_grid_row_dsc_array_1, 0);
    lv_obj_set_style_layout(container, LV_LAYOUT_GRID, 0);
    lv_obj_set_size(container, 320, 240);
    lv_obj_set_style_bg_color(container, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    
    for(int i=0; i<4; i++) {
        tracks[i].slider = lv_slider_create(container);
        lv_slider_set_orientation(tracks[i].slider, LV_SLIDER_ORIENTATION_VERTICAL);    
        lv_obj_set_size(tracks[i].slider, 30, 140);    
        lv_obj_set_style_grid_cell_x_align(tracks[i].slider, LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_column_pos(tracks[i].slider, i, 0);
        lv_obj_set_style_grid_cell_y_align(tracks[i].slider, LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_row_pos(tracks[i].slider, 0, 0);        
        lv_slider_set_range(tracks[i].slider, 0, 127);
        
        tracks[i].btn = lv_btn_create(container);
        lv_obj_set_style_grid_cell_x_align(tracks[i].btn, LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_column_pos(tracks[i].btn, i, 0);
        lv_obj_set_style_grid_cell_y_align(tracks[i].btn, LV_GRID_ALIGN_CENTER, 0);
        lv_obj_set_style_grid_cell_row_pos(tracks[i].btn, 1, 0);        
        
        lv_obj_set_flag(tracks[i].btn, LV_OBJ_FLAG_CHECKABLE, true);
        lv_obj_set_state(tracks[i].btn, LV_STATE_CHECKED, true);
        lv_obj_set_style_border_width(tracks[i].btn, 0, 0);
        lv_obj_set_style_bg_color(tracks[i].btn, lv_color_hex(COL_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_color(tracks[i].btn, lv_color_hex(COL_ACCENT), LV_PART_MAIN | LV_STATE_CHECKED);

        lv_obj_t * label = lv_label_create(tracks[i].btn);
        lv_obj_set_align(label, LV_ALIGN_CENTER);
        lv_obj_set_style_text_color(label, lv_color_hex(COL_TEXT), 0);
        lv_label_set_text(label, itoa(i, new char[3], 10));

        tracks[i].last_cc = -1; // force the first real value to be sent
        arm_pickup(tracks[i]); // start locked until the pot picks each track up
    }

    col_highlight = lv_obj_create(container);


#ifdef MIDI_WIFI
    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    midiHandler.addTransport(&rtpMIDI);
    rtpMIDI.begin();

#ifdef WOKWI
    // Needed for Wokwi to connect to the free Wokwi gateway (host.wokwi.internal) for AppleMIDI.
    // I don't know how or why but thanks Claude    
    IPAddress ip;
    if (WiFi.hostByName("host.wokwi.internal", ip))
        _rtpMidiInternal::Applemidi.sendInvite(ip, 5004);
#endif

#else
    midiHandler.addTransport(&usbMIDI);
    usbMIDI.begin();    
#endif

    midiHandler.begin();

    for(int i=0; i<N_INPUTS; i++) {
        pinMode(pot_pins[i], INPUT);
        pinMode(switch_pins[i], INPUT_PULLUP);
    }

    highlight_column(selected_column);

    left_state = digitalRead(switch_pins[0]);
    right_state = digitalRead(switch_pins[2]);

    // Serial.begin(115200);
    // Serial.println("USB Serial connection established successfully!");
}

void loop() {
    midiHandler.task(); /* Service transports — required for AppleMIDI session handshake/keepalive */
    lv_timer_handler(); /* Update UI */
    delay(20);
    
    int new_left_state = digitalRead(switch_pins[0]);
    if(new_left_state != left_state) {
        select_column(selected_column - 1);
        delay(200); // Debounce delay
        left_state = new_left_state;
    }

    int new_right_state = digitalRead(switch_pins[2]);
    if(new_right_state != right_state) {
        select_column(selected_column + 1);
        delay(200); // Debounce delay
        right_state = new_right_state;
    }

    int pot_value = analogRead(pot_pins[1]);
    pot_value = map(pot_value, 0, 4095, 0, 127);
    sync_track_to_pot(tracks[selected_column], pot_value);

    int switch_value = digitalRead(switch_pins[1]);

    bool arm_btn_state = lv_obj_get_state(tracks[selected_column].btn) == LV_STATE_CHECKED ? true : false;
    bool new_arm_btn_state = (switch_value == HIGH) ? true : false;

    if(new_arm_btn_state != arm_btn_state) {
        lv_obj_set_state(tracks[selected_column].btn, LV_STATE_CHECKED, new_arm_btn_state);
        midiHandler.sendControlChange(2, selected_column + 1, new_arm_btn_state ? 127 : 0);
    }
}
