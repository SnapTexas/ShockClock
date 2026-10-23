#include <Arduino.h>
#include <map>
#include <tuple>
#include <vector>
#include <stack>
#include <algorithm>
#include <Preferences.h>

#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

Preferences prefs;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
    U8G2_R0,
    U8X8_PIN_NONE
);

RTC_DS3231 rtc;

float debounce_time=0.3;

#define speaker_on_pin 18
#define speaker_music_pin 5
bool speaker_is_on = false;


unsigned long speaker_stage_start = 0;


bool shock_previous_state=false;
// OLED settings
#define oled_sda_pin 21
#define oled_scl_pin 22
#define oled_height 64
#define oled_width 128
#define origin_x_shift 5

std::vector<String> path;

volatile unsigned long last_button_time = 0;
bool alarm_ringing = false;
int set_alarm_hour = 0;
int set_alarm_minute = 0;
bool alarm_dismissed = false;
String where_are_we = "";
bool menu_currently_open = false;

volatile uint8_t current_menu_options = 3;
volatile uint32_t menu_index = 0;
volatile uint32_t menu_button_count = 0;
volatile int activity_count = 0;
int last_index=-1;
int last_time=-1;
volatile bool menu_button_event = false;

bool menu_button_pressed = false;

#define menu_label_height 30
const uint8_t max_options = 4;

#define no_option "NO_OPTION_SELECTED"

#define relay_pin 23
#define no_alarm 4294967295

uint32_t alarm_time;
uint32_t current_time;

// Encoder pins
#define encoder_button_pin 32
#define encoder_clk_pin 33
//#define encoder_dt_pin 33

// Matrix pins
#define matrix_data_pin 13
#define matrix_clock_pin 27
#define matrix_cs_pin 14

#define display_time "display"
#define menu "Menu"
#define alarm "Alarm"
#define mode "Mode"
#define back_option "back"

#define action_hour "hour"
#define action_minute "minute"
#define action_set_alarm "set"
#define action_delete_alarm "delete"
#define action_normal_mode "normal"
#define action_crazy_mode "crazy"
#define action_safe_mode "safe"

int last_display_selection_value=-1;

char current_mode;

#define normal_mode 'N'
#define safe_mode 'S'
#define crazy_mode 'C'
#define music_mode 'M'
#define default_mode safe_mode

std::map<char, std::map<String, bool>> allowed_things_in_this_mode;

volatile int current_index = 0;
volatile int pulse_count = 0;
volatile int direction = 0;


// NEW: Track whether the encoder is navigating a menu
// or selecting an hour/minute value.
enum SelectionState {
    NAVIGATING_MENU,
    SELECTING_HOUR,
    SELECTING_MINUTE
};

SelectionState selection_state = NAVIGATING_MENU;


// Menu definitions
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


// Function declarations
void relay_controller(bool state);
void set_alarm(int hour, int minute);
void alarm_gone_off();
void init_alarm_time();
bool is_alarm_window();
void draw_thick_frame(uint8_t thickness);

std::tuple<uint8_t, uint8_t, uint8_t>
convert_to_hour_min_sec(uint32_t time_value);

void init_operation_mode();
void init_rtc();
void define_things_allowed_in_modes();

void IRAM_ATTR encoder_button_fun();
void IRAM_ATTR read_encoder();

void init_oled_display();
void display_menus(String choice);
void display_label(String label);
void display_options(const char* const* options, int size);
void display_alarm_mode_options();

// NEW: Draw the hour/minute selection screen.
void display_value_selection(String label, int value);

String option_selected(const char* const* options, size_t size);
void perform_action(String action);
void close_menu();
void get_current_time();
void display_current_time();
void set_mode(char new_mode);
void delete_alarm();

std::tuple<const char* const*, size_t> get_menu(String state);


// Setup
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
    //pinMode(encoder_dt_pin, INPUT_PULLUP);

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
}


// Get menu options
std::tuple<const char* const*, size_t> get_menu(String state) {

    if (state.equalsIgnoreCase(menu)) {
        return {main_menu.options, main_menu.option_count};
    }
    else if (state.equalsIgnoreCase(alarm)) {
        return {alarm_menu.options, alarm_menu.option_count};
    }
    else if (state.equalsIgnoreCase(mode)) {
        return {mode_menu.options, mode_menu.option_count};
    }

    return {nullptr, 0};
}


// Main loop
void loop() {

    // Process the button event FIRST.
    // This must happen before any selection-state return.
    get_current_time();
    if (menu_button_event) {
        noInterrupts();
        menu_button_event = false;
        interrupts();

        menu_button_pressed = true;

        Serial.println("Menu button event happened!");
    }

    // Re-arm the alarm once its trigger window has passed.
    if (alarm_dismissed && !is_alarm_window()) {
        alarm_dismissed = false;
    }

    if (menu_button_pressed && alarm_ringing) {

        alarm_dismissed = true;
        alarm_ringing = false;

        // Stop alarm hardware/effects.
        relay_controller(false);
        play_music(false);

        path.clear();
        menu_button_pressed = false;
        close_menu();

        get_current_time();

        display_current_time();
        draw_thick_frame(3);

        return;
    }

    // Check whether the alarm should start.
    get_current_time();

    if (!alarm_ringing && is_alarm_triggered()) {
        alarm_ringing = true;

        Serial.println("ALARM GONE OFF!");

        alarm_gone_off();
    }
    play_music(alarm_ringing);

    // NEW: Hour selection mode.
    if (selection_state == SELECTING_HOUR) {

        current_menu_options = 24;

        // Confirm the selected hour.
        if (menu_button_pressed) {

            set_alarm_hour = current_index;

            Serial.print("Selected hour: ");
            Serial.println(set_alarm_hour);

            selection_state = NAVIGATING_MENU;
            current_index = 0;
            last_index = -1;  // Force the menu to redraw
            menu_button_pressed = false;

            return;
        }

        display_value_selection("SET HOUR", current_index);

        return;
    }


    // NEW: Minute selection mode.
    if (selection_state == SELECTING_MINUTE) {

        current_menu_options = 60;

        // Confirm the selected minute.
        if (menu_button_pressed) {

            set_alarm_minute = current_index;

            Serial.print("Selected minute: ");
            Serial.println(set_alarm_minute);

            selection_state = NAVIGATING_MENU;
            current_index = 0;
            last_index = -1;  // Force the menu to redraw
            menu_button_pressed = false;

            return;
        }

        display_value_selection("SET MINUTE", current_index);

        return;
    }


    // Existing menu navigation.
    if (!path.empty()) {
        where_are_we = path.back();

        display_menus(where_are_we);
    }

    if (path.empty() && !menu_button_pressed) {

        Serial.println("Display_time");
        Serial.println(menu_button_pressed);
        get_current_time();

        if(last_time!=current_time/100){
          last_time=current_time/100;
          
          display_current_time();
          draw_thick_frame(3);
        }

    }
    else if (path.empty() && menu_button_pressed) {

        path.push_back(menu);
        current_index = 0;
        menu_button_pressed = false;

        return;
    }
    else {

        // Menu is open.
        if (menu_button_pressed) {

            auto [options, size_of_options] = get_menu(where_are_we);

            String choice = option_selected(
                options,
                size_of_options
            );

            auto found_at = std::find_if(
                main_menu.options,
                main_menu.options + main_menu.option_count,
                [&](const char* option) {
                    return choice.equalsIgnoreCase(option);
                }
            );

            if (choice.equalsIgnoreCase(back_option)) {

                if (path.size() <= 1) {
                    path.clear();
                    close_menu();
                }
                else {
                    path.pop_back();
                    current_index = 0;
                }

            }
            else if (
                found_at != main_menu.options + main_menu.option_count
            ) {

                path.push_back(choice);
                current_index = 0;

            }
            else {

                // Existing action handling.
                perform_action(choice);
                if (!choice.equalsIgnoreCase(action_hour) &&
                    !choice.equalsIgnoreCase(action_minute)) {
                    path.clear();
                    close_menu();
                }
            }
        }
    }

    menu_button_pressed = false;
}


// Actions

void alarm_gone_off() {
    Serial.println("Alarm is ringing!");
    play_music(true);
    relay_controller(true);

    
}

void draw_thick_frame(uint8_t thickness) {
    for (int i = 0; i < thickness; i++) {
        oled.drawFrame(i+thickness, i+thickness, 128-thickness - (2 * i), 64-thickness - (2 * i));
    }
    oled.sendBuffer();
}

void perform_action(String action) {

    if (action.equalsIgnoreCase(action_hour)) {

        selection_state = SELECTING_HOUR;
        current_menu_options = 24;
        current_index = set_alarm_hour;

    }
    else if (action.equalsIgnoreCase(action_minute)) {

        selection_state = SELECTING_MINUTE;
        current_menu_options = 60;
        current_index = set_alarm_minute;

    }
    else if (action.equalsIgnoreCase(action_set_alarm)) {

        set_alarm(set_alarm_hour, set_alarm_minute);
        Serial.println("Alarm set");
        
    }
    else if (action.equalsIgnoreCase(action_delete_alarm)) {

        delete_alarm();
        Serial.println("Alarm deleted");
        
    }
    else if (action.equalsIgnoreCase(action_normal_mode)) {

        set_mode(normal_mode);
        Serial.println("Normal mode selected");

    }
    else if (action.equalsIgnoreCase(action_crazy_mode)) {

        set_mode(crazy_mode);
        Serial.println("Crazy mode selected");

    }
    else if (action.equalsIgnoreCase(action_safe_mode)) {

        set_mode(safe_mode);
        Serial.println("Safe mode selected");

    }
    else {

        Serial.print("Unknown action: ");
        Serial.println(action);

    }
}

bool is_alarm_window() {
    if (alarm_time == no_alarm) return false;

    auto [alarm_hour, alarm_minute, alarm_second] =
        convert_to_hour_min_sec(alarm_time);

    auto [current_hour, current_minute, current_second] =
        convert_to_hour_min_sec(current_time);

    return current_hour == alarm_hour &&
           current_minute >= alarm_minute &&
           current_minute <= alarm_minute + 5;
}

// Display menus
void display_menus(String choice) {
        if (current_index != last_index) {
            last_index = current_index;
            if (choice.equalsIgnoreCase(menu)) {

                auto [options, size_of_options] = get_menu(menu);

                display_label(menu);
                display_options(options, size_of_options);

            }
            else if (choice.equalsIgnoreCase(mode)) {

                display_label(mode);

                auto [options, size_of_options] = get_menu(mode);

                display_options(options, size_of_options);

            }
            else if (choice.equalsIgnoreCase(alarm)) {

                display_label(alarm);
                display_alarm_mode_options();

            }
            

        }
    draw_thick_frame(3);

    Serial.println(digitalRead(encoder_button_pin));
}


// NEW: Display the selected hour or minute.
void display_value_selection(String label, int value) {
    if(last_display_selection_value!=value){
      oled.clearBuffer();
      last_display_selection_value=value;
    }

    oled.setFont(u8g2_font_9x15B_tf);
    draw_thick_frame(3);
    int label_width = oled.getStrWidth(label.c_str());
    int label_x = (oled_width - label_width) / 2;

    oled.drawStr(label_x, 20, label.c_str());

    // Display values as 00, 01, 02 ... 23 or 59.
    String value_text = (value < 10 ? "0" : "") + String(value);

    oled.setFont(u8g2_font_logisoso32_tn);

    int value_width = oled.getStrWidth(value_text.c_str());
    int value_x = (oled_width - value_width) / 2;

    oled.drawStr(value_x, 57, value_text.c_str());

    oled.sendBuffer();
}


// Relay controller
void relay_controller(bool state) {

    if (shock_previous_state == state) {
        return;
    }

    digitalWrite(relay_pin, state ? HIGH : LOW);
    shock_previous_state = state;
}

void play_music(bool state) {

    Serial.println("Speaker ON");
    if (state ^ speaker_is_on){
      // Press the speaker power button for 5 seconds.
      digitalWrite(speaker_on_pin, HIGH);
      delay(5000);
      digitalWrite(speaker_on_pin, LOW);

      speaker_is_on = state;
      delay(3000);
      digitalWrite(speaker_music_pin, HIGH);
      delay(5000);
      digitalWrite(speaker_music_pin, LOW);

    }


}
// Encoder button interrupt
void IRAM_ATTR encoder_button_fun() {

    //unsigned long now = micros();

    //if (now - last_button_time < 500000UL) {
    //    return;
    //}
    //last_button_time=now;
    activity_count++;
    menu_button_count++;
    menu_button_event = true;
}


// Close menu
void close_menu() {

    menu_currently_open = false;

    activity_count = 0;
    current_index = 0;
    pulse_count = 0;
    direction = 0;
    menu_index = 0;
    menu_button_count = 0;

    // NEW: Ensure we return to normal navigation.
    selection_state = NAVIGATING_MENU;
}


// Encoder rotation interrupt
void IRAM_ATTR read_encoder() {

    activity_count += 1;

    if (current_menu_options == 0) {
        return;
    }

    // Kept as in your original code.
    int new_direction = 1;

    if (new_direction != direction) {
        pulse_count = 0;
        direction = new_direction;
    }

    pulse_count++;

    // Count 10 pulses as one step.
    if (pulse_count >= 10) {

        pulse_count = 0;

        current_index =
            (current_index + direction + current_menu_options)
            % current_menu_options;

    }
}


// Save alarm
void set_alarm(int hour, int minute) {
    alarm_dismissed = false;
    if (!prefs.begin("alarm", false)) {
        Serial.println("Failed to open alarm namespace");
        return;
    }

    alarm_time = hour * 10000UL + minute * 100UL;

    prefs.putUInt("time", alarm_time);
    prefs.end();

    Serial.print("Alarm Set: ");
    Serial.println(alarm_time);
}


// Save mode
void set_mode(char new_mode) {

    if (!prefs.begin("saved_mode", false)) {
        Serial.println("Failed to open mode namespace");
        return;
    }

    prefs.putUChar(
        "saved_mode",
        static_cast<uint8_t>(new_mode)
    );

    prefs.end();

    current_mode = new_mode;

    Serial.print("Mode saved: ");
    Serial.println(current_mode);
}


// Delete alarm
void delete_alarm() {

    if (!prefs.begin("alarm", false)) {
        Serial.println("Failed to open alarm namespace");
        return;
    }

    prefs.remove("time");
    prefs.end();

    alarm_time = no_alarm;

    Serial.println("Alarm deleted");
}


// Initialize alarm
void init_alarm_time() {

    if (!prefs.begin("alarm", true)) {
        Serial.println("Failed to initialize alarm time");
        alarm_time = no_alarm;
        return;
    }

    alarm_time = prefs.getUInt("time", no_alarm);
    prefs.end();

    Serial.print("Alarm Time: ");

    if (alarm_time == no_alarm) {
        Serial.println("No alarm set");
    }
    else {
        Serial.println(alarm_time);
    }
}


// Get current time
void get_current_time() {

    DateTime now = rtc.now();

    current_time =
        now.hour() * 10000UL
        + now.minute() * 100UL
        + now.second();
}


// Display current time
void display_current_time() {

    oled.setFont(u8g2_font_helvB18_tf);

    auto [hour, minute, second] =
        convert_to_hour_min_sec(current_time);

    String time_text = String(hour) + " : " + String(minute);
    String mode_text = "mode :" + String(current_mode);

    oled.clearBuffer();

    oled.drawStr(35, 35, time_text.c_str());

    oled.setFont(u8g2_font_7x14_tf);

    oled.drawStr(70, 60, mode_text.c_str());

    oled.sendBuffer();
}


// Convert numeric time to hour, minute and second
std::tuple<uint8_t, uint8_t, uint8_t>
convert_to_hour_min_sec(uint32_t time_value) {

    uint8_t hour = time_value / 10000;
    uint8_t minute = (time_value / 100) % 100;
    uint8_t second = time_value % 100;

    return {hour, minute, second};
}


// Define permissions for each mode
void define_things_allowed_in_modes() {

    // Normal mode
    allowed_things_in_this_mode[normal_mode]["shock"] = true;
    allowed_things_in_this_mode[normal_mode]["public_access"] = false;
    allowed_things_in_this_mode[normal_mode]["speaker"] = true;
    allowed_things_in_this_mode[normal_mode]["set_alarm"] = true;
    allowed_things_in_this_mode[normal_mode]["power_saving"] = false;

    // Safe mode
    allowed_things_in_this_mode[safe_mode]["shock"] = false;
    allowed_things_in_this_mode[safe_mode]["public_access"] = false;
    allowed_things_in_this_mode[safe_mode]["speaker"] = true;
    allowed_things_in_this_mode[safe_mode]["set_alarm"] = false;
    allowed_things_in_this_mode[safe_mode]["power_saving"] = true;

    // Crazy mode
    allowed_things_in_this_mode[crazy_mode]["shock"] = true;
    allowed_things_in_this_mode[crazy_mode]["public_access"] = true;
    allowed_things_in_this_mode[crazy_mode]["speaker"] = true;
    allowed_things_in_this_mode[crazy_mode]["set_alarm"] = true;
    allowed_things_in_this_mode[crazy_mode]["power_saving"] = false;

    // Music mode
    allowed_things_in_this_mode[music_mode]["shock"] = false;
    allowed_things_in_this_mode[music_mode]["public_access"] = false;
    allowed_things_in_this_mode[music_mode]["speaker"] = true;
    allowed_things_in_this_mode[music_mode]["set_alarm"] = false;
    allowed_things_in_this_mode[music_mode]["power_saving"] = false;
}


// Initialize saved mode
void init_operation_mode() {

    if (!prefs.begin("saved_mode", true)) {
        Serial.println(
            "Failed to retrieve saved mode; reverting to default"
        );

        current_mode = default_mode;
        return;
    }

    current_mode = static_cast<char>(
        prefs.getUChar(
            "saved_mode",
            static_cast<uint8_t>(default_mode)
        )
    );

    prefs.end();

    Serial.print("Current Mode: ");
    Serial.println(current_mode);
}


// Initialize OLED
void init_oled_display() {

    Wire.begin(oled_sda_pin, oled_scl_pin);

    oled.begin();
    oled.clearBuffer();

    oled.setFont(u8g2_font_ncenB14_tr);
}


// Check alarm trigger
bool is_alarm_triggered() {

    if (alarm_time == no_alarm || alarm_dismissed) {
        return false;
    }

    auto [alarm_hour, alarm_minute, alarm_second] =
        convert_to_hour_min_sec(alarm_time);

    auto [current_hour, current_minute, current_second] =
        convert_to_hour_min_sec(current_time);

    if (current_hour == alarm_hour &&
        current_minute >= alarm_minute &&
        current_minute <= alarm_minute + 5) {
        return true;
    }

    return false;
}



// Initialize RTC
void init_rtc() {

    if (!rtc.begin()) {
        Serial.println("RTC NOT FOUND");

        while (true) {
            delay(100);
        }
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power; set its time!");
    }

    Serial.println("RTC FOUND");
}
void display_options(const char* const* options, int size) {
    if (size < 1 || size > max_options ||
        current_index < 0 || current_index >= size) {
        return;
    }

    current_menu_options = size;

    const int columns = 2;
    const int margin = 2;
    const int padding = 4;
    const int radius = 3;
    const int y_shift_options = -5;

    int max_height = oled.getMaxCharHeight();
    int max_width = 0;

    // Find the widest option.
    for (int i = 0; i < size; i++) {
        int text_width = oled.getStrWidth(options[i]);

        if (text_width > max_width) {
            max_width = text_width;
        }
    }

    // Every selection box gets the same maximum dimensions.
    int box_width = max_width + padding * 2;
    int box_height = max_height + padding;

    int start_y = menu_label_height + margin + y_shift_options;

    for (int i = 0; i < size; i++) {
        int col = i % columns;
        int row = i / columns;

        int option_x = origin_x_shift + col * (box_width + margin);
        int option_y = start_y + row * (box_height + margin);

        // Draw the selection box.
        if (i == current_index) {
            oled.drawRFrame(
                option_x,
                option_y,
                box_width,
                box_height,
                radius
            );
        }

        // Center the text inside its box.
        int text_width = oled.getStrWidth(options[i]);
        int text_x = option_x + (box_width - text_width) / 2;

        int text_height = oled.getAscent() - oled.getDescent();
        int text_y = option_y
                   + (box_height - text_height) / 2
                   + oled.getAscent();

        oled.drawStr(text_x, text_y, options[i]);
    }

    // Draw the outer frame before sending the completed screen.
    draw_thick_frame(3);
    oled.sendBuffer();
}

// Display label
void display_label(String label) {

    oled.clearBuffer();

    oled.setFont(u8g2_font_9x15B_tf);

    int max_char_height = oled.getMaxCharHeight();
    int width_of_label = oled.getStrWidth(label.c_str());

    int label_x = 5;
    int label_y = 20;

    if (width_of_label < oled_width) {
        label_x = (oled_width - width_of_label) / 2;
    }

    oled.drawStr(label_x, label_y, label.c_str());

    oled.sendBuffer();
}


// Get selected option
String option_selected(const char* const* options, size_t size) {

    if (
        options == nullptr
        || size == 0
        || current_index < 0
        || current_index >= size
    ) {
        return "";
    }

    return String(options[current_index]);
}


// Display alarm menu
void display_alarm_mode_options() {

    // IMPORTANT: Don't reset the encoder range while selecting a value.
    if (selection_state == NAVIGATING_MENU) {
        current_menu_options = alarm_menu.option_count;
    }

    int margin = 2;
    int radius = 3;

    oled.setFont(u8g2_font_6x10_tf);

    // Calculate maximum option width.
    int max_option_width = 0;

    for (int i = 0; i < alarm_menu.option_count; i++) {

        int curr_option_width =
            oled.getStrWidth(alarm_menu.options[i]);

        if (max_option_width < curr_option_width) {
            max_option_width = curr_option_width;
        }
    }

    int alarm_option_max_width =
        max_option_width + 2 * margin;

    // Position and dimensions.
    int x1 = origin_x_shift + margin + 5;
    int y1 = menu_label_height + margin;

    int width = 25;
    int height = 20;
    int gap = 4;

    // Display selected hour.
    if (current_index == 0) {

        oled.drawRBox(x1, y1, width, height, radius);
        oled.setDrawColor(0);

    }
    else {

        oled.drawRFrame(x1, y1, width, height, radius);
        oled.setDrawColor(1);

    }

    String hour_text = String(set_alarm_hour);

    if (set_alarm_hour < 10) {
        hour_text = "0" + hour_text;
    }

    oled.drawStr(x1 + 6, y1 + 14, hour_text.c_str());

    oled.setDrawColor(1);

    // Display selected minute.
    int x2 = x1 + width + gap;

    if (current_index == 1) {

        oled.drawRBox(x2, y1, width, height, radius);
        oled.setDrawColor(0);

    }
    else {

        oled.drawRFrame(x2, y1, width, height, radius);
        oled.setDrawColor(1);

    }

    String minute_text = String(set_alarm_minute);

    if (set_alarm_minute < 10) {
        minute_text = "0" + minute_text;
    }

    oled.drawStr(x2 + 6, y1 + 14, minute_text.c_str());

    oled.setDrawColor(1);

    // Display Set, Delete and Back.
    int action_gap = 5;
    int alarm_options_x = x2 + width + action_gap;
    int action_height = 10;

    for (int i = 2; i < alarm_menu.option_count; i++) {

        int row = i - 2;
        int row_y = y1 + row * action_height;

        if (current_index == i) {

            oled.drawRFrame(
                alarm_options_x - margin,
                row_y,
                alarm_option_max_width,
                action_height,
                radius
            );

        }

        oled.drawStr(
            alarm_options_x,
            row_y + 8,
            alarm_menu.options[i]
        );
    }

    oled.sendBuffer();
}