#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <vector>
#include <map>
#include <tuple>
#include <algorithm>
#include <MD_MAX72xx.h>



// ============================================================
// HARDWARE
// ============================================================

#define speaker_on_pin       18
#define speaker_music_pin     5
#define relay_pin            23

#define oled_sda_pin         21
#define oled_scl_pin         22
#define oled_width          128
#define oled_height          64

#define encoder_button_pin   32
#define encoder_clk_pin      33

#define matrix_data_pin      13
#define matrix_clock_pin     27
#define matrix_cs_pin        14



#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

MD_MAX72XX lc = MD_MAX72XX(
    HARDWARE_TYPE,
    matrix_data_pin,
    matrix_clock_pin,
    matrix_cs_pin,
    1
);

byte skullBlink0[8] = {
  B00111100, B01111110, B11111111, B11111111,
  B11110111, B01111110, B00111110, B00101010
};

byte skullBlink1[8] = {
  B00111100, B01111110, B11111111, B11001001,
  B11110111, B01111110, B00111110, B00101010
};

byte skullBlink2[8] = {
  B00111100, B01111110, B11001001, B11001001,
  B11110111, B01111110, B00111110, B00101010
};

byte skull0[8] = {
  B00111100, B01111110, B11001001, B11011011,
  B11110111, B01111110, B00111110, B00101010
};

byte skull1[8] = {
  B00111100, B01111110, B11011011, B11001001,
  B11110111, B01111110, B00111110, B00101010
};

byte skull3[8] = {
  B00111100, B01111110, B11001001, B11101101,
  B11110111, B01111110, B00111110, B00101010
};

byte* frames[] = {
  skullBlink2, skullBlink1, skullBlink0,
  skullBlink1, skullBlink2, skull0,
  skull1, skull3, skull0
};

const unsigned int frameTime[] = {
  500, 200, 500, 200, 200, 300, 300, 300, 250
};

const byte TOTAL_FRAMES = sizeof(frames) / sizeof(frames[0]);

byte rotated[8];
byte currentFrame = 0;
unsigned long previousMillis = 0;

unsigned long speaker_on_start = 0;
unsigned long music_wait_start = 0;
unsigned long speaker_music_start = 0;

bool speaker_on_timing = false;
bool music_waiting = false;
bool speaker_music_timing = false;
bool music_target_state = false;

// ============================================================
// WIFI / MQTT
// ============================================================

const char* WIFI_SSID = "SHOAIB";
const char* WIFI_PASSWORD = "Bismillah@0121";

const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_TOPIC = "snapclock-00989800/control";

WiFiClientSecure mqttSecureClient;
PubSubClient mqttClient(mqttSecureClient);

unsigned long lastMqttAttempt = 0;
bool remoteAlarmDisplay = false;
unsigned long remoteAlarmStarted = 0;
const unsigned long REMOTE_ALARM_DURATION = 10000UL;

// ============================================================
// GLOBAL OBJECTS
// ============================================================

Preferences prefs;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
    U8G2_R0,
    U8X8_PIN_NONE
);

RTC_DS3231 rtc;

std::vector<String> path;

std::map<char, std::map<String, bool>>
    allowed_things_in_this_mode;

// ============================================================
// SETTINGS / STATE
// ============================================================

#define origin_x_shift 5
#define menu_label_height 30
#define no_alarm 4294967295UL

#define normal_mode 'N'
#define safe_mode   'S'
#define crazy_mode  'C'
#define music_mode  'M'
#define default_mode safe_mode

#define MENU_NAME  "Menu"
#define ALARM_NAME "Alarm"
#define MODE_NAME  "Mode"
#define BACK_NAME  "Back"

#define ACTION_HOUR   "Hour"
#define ACTION_MINUTE "Minute"
#define ACTION_SET    "Set"
#define ACTION_DELETE "Delete"
#define ACTION_NORMAL "Normal"
#define ACTION_SAFE   "Safe"
#define ACTION_CRAZY  "Crazy"

uint32_t alarm_time = no_alarm;
uint32_t current_time = 0;

char current_mode = default_mode;

int set_alarm_hour = 0;
int set_alarm_minute = 0;

bool alarm_ringing = false;
bool alarm_dismissed = false;
bool menu_button_pressed = false;
bool menu_currently_open = false;

bool speaker_is_on = false;
bool shock_previous_state = false;

volatile bool menu_button_event = false;
volatile unsigned long last_button_time = 0;

volatile int current_index = 0;
volatile int current_menu_options = 3;
volatile int pulse_count = 0;
volatile int direction = 0;
volatile int activity_count = 0;

int last_index = -1;
int last_time = -1;
int last_display_selection_value = -1;

float debounce_time = 0.3f;

// ============================================================
// MENU DEFINITIONS
// ============================================================

enum SelectionState {
    NAVIGATING_MENU,
    SELECTING_HOUR,
    SELECTING_MINUTE
};

SelectionState selection_state = NAVIGATING_MENU;

enum MenuID {
    MAIN_MENU,
    ALARM_MENU,
    MODE_MENU
};

struct Menu {
    MenuID id;
    const char* title;
    const char* options[5];
    uint8_t option_count;
};

Menu main_menu = {
    MAIN_MENU,
    "MENU",
    {"Alarm", "Mode", "Back"},
    3
};

Menu alarm_menu = {
    ALARM_MENU,
    "ALARM",
    {"Hour", "Minute", "Set", "Delete", "Back"},
    5
};

Menu mode_menu = {
    MODE_MENU,
    "MODE",
    {"Normal", "Safe", "Crazy", "Back"},
    4
};

// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

void initMatrixAnimation();
void updateAnimation();
void drawRotated(byte frame[]);

void init_rtc();
void init_oled_display();
void init_alarm_time();
void init_operation_mode();
void define_things_allowed_in_modes();

void get_current_time();
void display_current_time();
void display_remote_alarm();
void display_label(String label);
void display_options(const char* const* options, int size);
void display_alarm_mode_options();
void display_value_selection(String label, int value);
void display_menus(String choice);
void draw_thick_frame(uint8_t thickness);

void set_alarm(int hour, int minute);
void delete_alarm();
void set_mode(char new_mode);
void perform_action(String action);
void close_menu();

void relay_controller(bool state);
void play_music(bool state);
void alarm_gone_off();

bool is_alarm_window();
bool is_alarm_triggered();

void encoder_button_fun();
void read_encoder();

void connect_wifi();
void setup_mqtt();
void service_mqtt();
void mqtt_callback(char* topic, byte* payload, unsigned int length);

std::tuple<uint8_t, uint8_t, uint8_t>
convert_to_hour_min_sec(uint32_t time_value);

std::tuple<const char* const*, size_t>
get_menu(String state);

String option_selected(const char* const* options, size_t size);

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);

    pinMode(relay_pin, OUTPUT);
    digitalWrite(relay_pin, LOW);
    shock_previous_state = false;

    pinMode(speaker_on_pin, OUTPUT);
    pinMode(speaker_music_pin, OUTPUT);

    digitalWrite(speaker_on_pin, LOW);
    digitalWrite(speaker_music_pin, LOW);

    pinMode(encoder_button_pin, INPUT_PULLUP);
    pinMode(encoder_clk_pin, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(encoder_button_pin),
        encoder_button_fun,
        FALLING
    );

    attachInterrupt(
        digitalPinToInterrupt(encoder_clk_pin),
        read_encoder,
        CHANGE
    );

    define_things_allowed_in_modes();

    init_rtc();
    init_oled_display();
    init_alarm_time();
    init_operation_mode();
    initMatrixAnimation();
    connect_wifi();
    setup_mqtt();

    get_current_time();
    display_current_time();
    draw_thick_frame(3);

    Serial.println("Clock initialized");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
    service_mqtt();
    get_current_time();

    if (menu_button_event) {
        noInterrupts();
        menu_button_event = false;
        interrupts();

        menu_button_pressed = true;
    }
    if(alarm_ringing){
        display_remote_alarm();
        updateAnimation();
    }
    else{
        lc.clear();
    }   
    // Re-arm once the current alarm window has passed.
    if (alarm_dismissed && !is_alarm_window()) {
        alarm_dismissed = false;
    }

    // A remote MQTT message displays Alarm for 10 seconds.
    // It does not activate the relay.


    // Dismiss the local alarm with the encoder button.
    if (menu_button_pressed && alarm_ringing) {
        alarm_dismissed = true;
        alarm_ringing = false;
        remoteAlarmDisplay = false;
        menu_button_pressed = false;
        last_time = -1;
        last_index = -1;
        relay_controller(false);
        play_music(false);

        
        path.clear();
        close_menu();

        get_current_time();
        display_current_time();
        draw_thick_frame(3);
        return;
    }

    // Trigger the saved RTC alarm.
    if (!alarm_ringing && (is_alarm_triggered() || remoteAlarmDisplay)) {
            

            

        draw_thick_frame(3);
      
        
    
        alarm_ringing = true;
        Serial.println("ALARM GONE OFF!");
        alarm_gone_off();
    }

    //play_music(alarm_ringing);

    // Hour selection.
    if (selection_state == SELECTING_HOUR) {
        current_menu_options = 24;

        if (menu_button_pressed) {
            set_alarm_hour = current_index;
            selection_state = NAVIGATING_MENU;
            current_index = 0;
            last_index = -1;
            last_display_selection_value = -1;
            menu_button_pressed = false;
            return;
        }

        display_value_selection("SET HOUR", current_index);
        return;
    }

    // Minute selection.
    if (selection_state == SELECTING_MINUTE) {
        current_menu_options = 60;

        if (menu_button_pressed) {
            set_alarm_minute = current_index;
            selection_state = NAVIGATING_MENU;
            current_index = 0;
            last_index = -1;
            last_display_selection_value = -1;
            menu_button_pressed = false;
            return;
        }

        display_value_selection("SET MINUTE", current_index);
        return;
    }

    // Menu navigation.
    if (!path.empty()) {
        display_menus(path.back());
    }

    if (path.empty() && !menu_button_pressed) {
        if (last_time != (int)(current_time / 100)) {
            last_time = current_time / 100;
            display_current_time();
            draw_thick_frame(3);
        }
    } else if (path.empty() && menu_button_pressed) {
        path.push_back(MENU_NAME);
        current_index = 0;
        current_menu_options = main_menu.option_count;
        last_index = -1;
        menu_button_pressed = false;
        return;
    } else if (!path.empty() && menu_button_pressed) {
        String where_are_we = path.back();

        auto [options, count] = get_menu(where_are_we);
        String choice = option_selected(options, count);

        if (choice.equalsIgnoreCase(BACK_NAME)) {
            if (path.size() <= 1) {
                path.clear();
                close_menu();
                display_current_time();
                draw_thick_frame(3);
            } else {
                path.pop_back();
                current_index = 0;
                last_index = -1;

                if (path.back().equalsIgnoreCase(ALARM_NAME)) {
                    current_menu_options = alarm_menu.option_count;
                } else if (path.back().equalsIgnoreCase(MODE_NAME)) {
                    current_menu_options = mode_menu.option_count;
                } else {
                    current_menu_options = main_menu.option_count;
                }
            }
        } else if (where_are_we.equalsIgnoreCase(MENU_NAME) &&
                   (choice.equalsIgnoreCase(ALARM_NAME) ||
                    choice.equalsIgnoreCase(MODE_NAME))) {
            path.push_back(choice);
            current_index = 0;
            last_index = -1;

            if (choice.equalsIgnoreCase(ALARM_NAME)) {
                current_menu_options = alarm_menu.option_count;
            } else {
                current_menu_options = mode_menu.option_count;
            }
        } else {
            perform_action(choice);

            if (selection_state == NAVIGATING_MENU) {
                path.clear();
                close_menu();
                display_current_time();
                draw_thick_frame(3);
            }
        }
    }

    menu_button_pressed = false;
}

// ============================================================
// MENU LOOKUP
// ============================================================



void drawRotated(byte frame[]) {
    for (byte i = 0; i < 8; i++) {
        rotated[i] = 0;
    }

    // Rotate 90 degrees clockwise
    for (byte r = 0; r < 8; r++) {
        for (byte c = 0; c < 8; c++) {
            if (frame[r] & (1 << (7 - c))) {
                rotated[c] |= (1 << r);
            }
        }
    }

    for (byte row = 0; row < 8; row++) {
        lc.setRow(0, row, rotated[row]);
    }
}

void initMatrixAnimation() {
    lc.begin();
    lc.control(MD_MAX72XX::INTENSITY, 0);
    lc.clear();

    currentFrame = 0;
    previousMillis = millis();
}

void updateAnimation() {
  unsigned long now = millis();

  if (now - previousMillis >= frameTime[currentFrame]) {
    previousMillis = now;

    currentFrame++;
    if (currentFrame >= TOTAL_FRAMES) {
      currentFrame = 0;
    }

    drawRotated(frames[currentFrame]);
  }
}


std::tuple<const char* const*, size_t> get_menu(String state) {
    if (state.equalsIgnoreCase(MENU_NAME)) {
        return {main_menu.options, main_menu.option_count};
    }

    if (state.equalsIgnoreCase(ALARM_NAME)) {
        return {alarm_menu.options, alarm_menu.option_count};
    }

    if (state.equalsIgnoreCase(MODE_NAME)) {
        return {mode_menu.options, mode_menu.option_count};
    }

    return {nullptr, 0};
}

String option_selected(const char* const* options, size_t size) {
    if (options == nullptr || size == 0 ||
        current_index < 0 || current_index >= (int)size) {
        return "";
    }

    return String(options[current_index]);
}

// ============================================================
// ACTIONS
// ============================================================

void perform_action(String action) {
    if (action.equalsIgnoreCase(ACTION_HOUR)) {
        selection_state = SELECTING_HOUR;
        current_menu_options = 24;
        current_index = set_alarm_hour;
        last_display_selection_value = -1;
    } else if (action.equalsIgnoreCase(ACTION_MINUTE)) {
        selection_state = SELECTING_MINUTE;
        current_menu_options = 60;
        current_index = set_alarm_minute;
        last_display_selection_value = -1;
    } else if (action.equalsIgnoreCase(ACTION_SET)) {
        set_alarm(set_alarm_hour, set_alarm_minute);
    } else if (action.equalsIgnoreCase(ACTION_DELETE)) {
        delete_alarm();
    } else if (action.equalsIgnoreCase(ACTION_NORMAL)) {
        set_mode(normal_mode);
    } else if (action.equalsIgnoreCase(ACTION_SAFE)) {
        set_mode(safe_mode);
    } else if (action.equalsIgnoreCase(ACTION_CRAZY)) {
        set_mode(crazy_mode);
    } else {
        Serial.print("Unknown action: ");
        Serial.println(action);
    }
}

void alarm_gone_off() {
    Serial.println("Local RTC alarm is ringing");

    //play_music(true);

    // Safety: relay activation is deliberately disabled.
    // Keep the relay output LOW in this version.
    relay_controller(true);
}

void close_menu() {
    menu_currently_open = false;
    current_index = 0;
    pulse_count = 0;
    direction = 0;
    selection_state = NAVIGATING_MENU;
    current_menu_options = main_menu.option_count;
    last_index = -1;
    last_display_selection_value = -1;
}

// ============================================================
// RTC ALARM SETTINGS
// ============================================================

void set_alarm(int hour, int minute) {
    alarm_dismissed = false;

    if (!prefs.begin("alarm", false)) {
        Serial.println("Failed to open alarm namespace");
        return;
    }

    alarm_time = hour * 10000UL + minute * 100UL;
    prefs.putUInt("time", alarm_time);
    prefs.end();

    Serial.print("Alarm set to ");
    Serial.print(hour);
    Serial.print(":");
    Serial.println(minute);
}

void delete_alarm() {
    if (!prefs.begin("alarm", false)) {
        Serial.println("Failed to open alarm namespace");
        return;
    }

    prefs.remove("time");
    prefs.end();

    alarm_time = no_alarm;
    alarm_dismissed = false;

    Serial.println("Alarm deleted");
}

void init_alarm_time() {
    if (!prefs.begin("alarm", true)) {
        alarm_time = no_alarm;
        Serial.println("Could not load saved alarm");
        return;
    }

    alarm_time = prefs.getUInt("time", no_alarm);
    prefs.end();

    if (alarm_time == no_alarm) {
        Serial.println("No alarm saved");
    } else {
        Serial.print("Saved alarm: ");
        Serial.println(alarm_time);
    }
}

bool is_alarm_window() {
    if (alarm_time == no_alarm) {
        return false;
    }

    auto [ah, am, as] = convert_to_hour_min_sec(alarm_time);
    auto [ch, cm, cs] = convert_to_hour_min_sec(current_time);

    return ch == ah && cm >= am && cm <= am + 5;
}

bool is_alarm_triggered() {
    if (alarm_time == no_alarm || alarm_dismissed) {
        return false;
    }

    return is_alarm_window();
}

// ============================================================
// MODE SETTINGS
// ============================================================

void set_mode(char new_mode) {
    if (!prefs.begin("saved_mode", false)) {
        Serial.println("Failed to save mode");
        return;
    }

    prefs.putUChar("saved_mode", (uint8_t)new_mode);
    prefs.end();

    current_mode = new_mode;

    Serial.print("Mode saved: ");
    Serial.println(current_mode);
}

void init_operation_mode() {
    if (!prefs.begin("saved_mode", true)) {
        current_mode = default_mode;
        return;
    }

    current_mode = (char)prefs.getUChar(
        "saved_mode",
        (uint8_t)default_mode
    );

    prefs.end();

    Serial.print("Current mode: ");
    Serial.println(current_mode);
}

void define_things_allowed_in_modes() {
    allowed_things_in_this_mode[normal_mode]["shock"] = true;
    allowed_things_in_this_mode[normal_mode]["public_access"] = false;
    allowed_things_in_this_mode[normal_mode]["speaker"] = true;
    allowed_things_in_this_mode[normal_mode]["set_alarm"] = true;
    allowed_things_in_this_mode[normal_mode]["power_saving"] = false;

    allowed_things_in_this_mode[safe_mode]["shock"] = false;
    allowed_things_in_this_mode[safe_mode]["public_access"] = false;
    allowed_things_in_this_mode[safe_mode]["speaker"] = true;
    allowed_things_in_this_mode[safe_mode]["set_alarm"] = false;
    allowed_things_in_this_mode[safe_mode]["power_saving"] = true;

    allowed_things_in_this_mode[crazy_mode]["shock"] = true;
    allowed_things_in_this_mode[crazy_mode]["public_access"] = true;
    allowed_things_in_this_mode[crazy_mode]["speaker"] = true;
    allowed_things_in_this_mode[crazy_mode]["set_alarm"] = true;
    allowed_things_in_this_mode[crazy_mode]["power_saving"] = false;

    allowed_things_in_this_mode[music_mode]["shock"] = false;
    allowed_things_in_this_mode[music_mode]["public_access"] = false;
    allowed_things_in_this_mode[music_mode]["speaker"] = true;
    allowed_things_in_this_mode[music_mode]["set_alarm"] = false;
    allowed_things_in_this_mode[music_mode]["power_saving"] = false;
}

// ============================================================
// RTC / TIME DISPLAY
// ============================================================

void init_rtc() {
    if (!rtc.begin()) {
        Serial.println("RTC NOT FOUND");
        while (true) {
            delay(100);
        }
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power; set its time");
    }

    Serial.println("RTC FOUND");
}

void get_current_time() {
    DateTime now = rtc.now();

    current_time =
        now.hour() * 10000UL +
        now.minute() * 100UL +
        now.second();
}

std::tuple<uint8_t, uint8_t, uint8_t>
convert_to_hour_min_sec(uint32_t time_value) {
    uint8_t hour = time_value / 10000UL;
    uint8_t minute = (time_value / 100UL) % 100;
    uint8_t second = time_value % 100;

    return {hour, minute, second};
}

void init_oled_display() {
    Wire.begin(oled_sda_pin, oled_scl_pin);
    oled.begin();
    oled.clearBuffer();
    oled.sendBuffer();
}

void display_current_time() {
    auto [hour, minute, second] =
        convert_to_hour_min_sec(current_time);

    char timeText[16];
    snprintf(timeText, sizeof(timeText), "%02u:%02u", hour, minute);

    char modeText[16];
    snprintf(modeText, sizeof(modeText), "Mode: %c", current_mode);

    oled.clearBuffer();

    oled.setFont(u8g2_font_helvB18_tf);
    int timeWidth = oled.getStrWidth(timeText);
    oled.drawStr((oled_width - timeWidth) / 2, 35, timeText);

    oled.setFont(u8g2_font_7x14_tf);
    int modeWidth = oled.getStrWidth(modeText);
    oled.drawStr((oled_width - modeWidth) / 2, 56, modeText);

    oled.sendBuffer();
}

void display_remote_alarm() {
    oled.clearBuffer();

    // Bold, readable font; text begins at x=5, baseline y=30.
    oled.setFont(u8g2_font_helvB18_tf);
    oled.drawStr(5, 30, "Alarm");

    oled.sendBuffer();
}

void draw_thick_frame(uint8_t thickness) {
    for (int i = 0; i < thickness; i++) {
        oled.drawFrame(
            i + thickness,
            i + thickness,
            oled_width - thickness - (2 * i),
            oled_height - thickness - (2 * i)
        );
    }

    oled.sendBuffer();
}

// ============================================================
// DISPLAY MENUS
// ============================================================

void display_menus(String choice) {
    if (current_index != last_index) {
        last_index = current_index;

        if (choice.equalsIgnoreCase(MENU_NAME)) {
            display_label(MENU_NAME);
            auto [options, count] = get_menu(MENU_NAME);
            display_options(options, count);
        } else if (choice.equalsIgnoreCase(MODE_NAME)) {
            display_label(MODE_NAME);
            auto [options, count] = get_menu(MODE_NAME);
            display_options(options, count);
        } else if (choice.equalsIgnoreCase(ALARM_NAME)) {
            display_label(ALARM_NAME);
            display_alarm_mode_options();
        }
    }
}

void display_label(String label) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_9x15B_tf);

    int width = oled.getStrWidth(label.c_str());
    int x = (oled_width - width) / 2;

    oled.drawStr(x, 20, label.c_str());
    oled.sendBuffer();
}

void display_options(const char* const* options, int size) {
    if (options == nullptr || size < 1 || size > 5) {
        return;
    }

    current_menu_options = size;


    oled.setFont(u8g2_font_6x10_tf);

    const int columns = 2;
    const int left = 5;
    const int startY = 32;
    const int cellW = 59;
    const int cellH = 14;

    for (int i = 0; i < size; i++) {
        int col = i % columns;
        int row = i / columns;

        int x = left + col * cellW;
        int y = startY + row * cellH;

        if (i == current_index) {
            oled.drawRFrame(x, y, cellW - 3, cellH, 2);
        }

        oled.drawStr(x + 4, y + 10, options[i]);
    }

    for (int i = 0; i < 3; i++) {
        oled.drawFrame(i + 3, i + 3, 122 - 2 * i, 58 - 2 * i);
    }

    oled.sendBuffer();
}

void display_alarm_mode_options() {
    current_menu_options = alarm_menu.option_count;

    oled.clearBuffer();
    oled.setFont(u8g2_font_6x10_tf);

    oled.drawStr(5, 13, "ALARM");

    char hourText[4];
    char minuteText[4];

    snprintf(hourText, sizeof(hourText), "%02d", set_alarm_hour);
    snprintf(minuteText, sizeof(minuteText), "%02d", set_alarm_minute);

    if (current_index == 0) {
        oled.drawRFrame(4, 20, 29, 17, 2);
    }
    oled.drawStr(10, 32, hourText);

    if (current_index == 1) {
        oled.drawRFrame(35, 20, 29, 17, 2);
    }
    oled.drawStr(41, 32, minuteText);

    const int x = 72;
    const int yStart = 22;
    const int rowH = 11;

    for (int i = 2; i < alarm_menu.option_count; i++) {
        int y = yStart + (i - 2) * rowH;

        if (current_index == i) {
            oled.drawRFrame(x - 2, y - 8, 51, 11, 2);
        }

        oled.drawStr(x, y, alarm_menu.options[i]);
    }

    for (int i = 0; i < 3; i++) {
        oled.drawFrame(i + 3, i + 3, 122 - 2 * i, 58 - 2 * i);
    }

    oled.sendBuffer();
}

void display_value_selection(String label, int value) {
    oled.clearBuffer();

    oled.setFont(u8g2_font_9x15B_tf);
    int labelWidth = oled.getStrWidth(label.c_str());
    int labelX = (oled_width - labelWidth) / 2;

    oled.drawStr(labelX, 18, label.c_str());

    char valueText[4];
    snprintf(valueText, sizeof(valueText), "%02d", value);

    oled.setFont(u8g2_font_logisoso32_tn);
    int valueWidth = oled.getStrWidth(valueText);
    int valueX = (oled_width - valueWidth) / 2;

    oled.drawStr(valueX, 58, valueText);
    oled.sendBuffer();
}

// ============================================================
// ENCODER
// ============================================================

void encoder_button_fun() {
    unsigned long now = millis();

    if (now - last_button_time < 250) {
        return;
    }

    last_button_time = now;
    menu_button_event = true;
}

void read_encoder() {
    static int lastCLK = HIGH;

    int clk = digitalRead(encoder_clk_pin);
    int dt = digitalRead(encoder_button_pin); // Not used for direction.

    (void)dt;

    if (clk != lastCLK) {
        // This sketch only has the CLK pin configured.
        // Without the encoder DT pin, direction cannot be determined.
        // Rotate forward through the available choices.
        if (clk == LOW && current_menu_options > 0) {
            current_index =
                (current_index + 1) % current_menu_options;
        }
        lastCLK = clk;
    }
}

// ============================================================
// SPEAKER / RELAY
// ============================================================

void relay_controller(bool state) {

    if (shock_previous_state == state) {
        return;
    }
    digitalWrite(relay_pin, state ? HIGH : LOW);
    shock_previous_state = state;
}



void play_music(bool state) {
  unsigned long now = millis();

  if (state ^ speaker_is_on) {
    if (!speaker_on_timing && !music_waiting && !speaker_music_timing) {
      Serial.println("Speaker ON");

      music_target_state = state;
      digitalWrite(speaker_on_pin, HIGH);
      speaker_on_start = now;
      speaker_on_timing = true;
    }
  }

  if (speaker_on_timing && now - speaker_on_start >= 5000UL) {
    digitalWrite(speaker_on_pin, LOW);
    speaker_is_on = music_target_state;

    speaker_on_timing = false;
    music_wait_start = now;
    music_waiting = true;
  }

  if (music_waiting && now - music_wait_start >= 3000UL) {
    digitalWrite(speaker_music_pin, HIGH);

    music_waiting = false;
    speaker_music_start = now;
    speaker_music_timing = true;
  }

  if (speaker_music_timing && now - speaker_music_start >= 5000UL) {
    digitalWrite(speaker_music_pin, LOW);
    speaker_music_timing = false;
  }
}


// ============================================================
// MQTT
// ============================================================

void connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to Wi-Fi...");
}

void setup_mqtt() {
    // Development setting only. Production deployments should
    // validate the broker's TLS certificate.
    mqttSecureClient.setInsecure();

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
    mqttClient.setBufferSize(256);
}

void service_mqtt() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    if (!mqttClient.connected()) {
        if (millis() - lastMqttAttempt >= 5000UL) {
            lastMqttAttempt = millis();

            String clientId =
                "SnapClock-" +
                String((uint32_t)ESP.getEfuseMac(), HEX);

            if (mqttClient.connect(clientId.c_str())) {
                Serial.println("MQTT connected");

                if (!mqttClient.subscribe(MQTT_TOPIC)) {
                    Serial.println("MQTT subscribe failed");
                }
            } else {
                Serial.print("MQTT connection failed, state=");
                Serial.println(mqttClient.state());
            }
        }

        return;
    }

    mqttClient.loop();
}

void mqtt_callback(
    char* topic,
    byte* payload,
    unsigned int length
) {
    if (strcmp(topic, MQTT_TOPIC) != 0) {
        return;
    }

    if (length == 0 || length >= 80) {
        return;
    }

    char message[80];
    memcpy(message, payload, length);
    message[length] = '\0';

    if (strcmp(message, "{\"trigger\":true}") == 0) {
        remoteAlarmDisplay = true;
        remoteAlarmStarted = millis();

        

        Serial.println("Remote alarm display triggered");
    }
}