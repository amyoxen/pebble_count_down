// Countdown - A simple countdown timer for the Pebble watch and with charts to track the most recent records.

#include <pebble.h>
//#include "chart.h"
#define PERSIST_DATA 10
#define PERSIST_LAST_WDATE 100

//static uint32_t buffer_array[3];

static Window *window;

// Action Bar
ActionBarLayer *action_bar_layer;
static TextLayer *text_layer;
//static ChartLayer* chart_layer;
static TextLayer * title_text_layer;

char str_month[3];
char str_day[3];
char str_hour[3]; //---e.g. "15\0"---
char str_minute[3]; //---e.g. "59\0"---
char str_current_status[30]; //---store hr:min; e.g. 15:59\0---
char str_cp[3];
char str_center_status[30];
// Button data, based on the enum ButtonId in pebble.h. State (PRESENT/NOT PRESENT), bitmap
// if the button is PRESENT
typedef enum { NOT_PRESENT, PRESENT } ButtonImageState;
typedef struct {
    ButtonImageState state;
    GBitmap *bitmap;
} ButtonData;
ButtonData button_data[NUM_BUTTONS] = {
                                        { NOT_PRESENT, NULL }, 
                                        { NOT_PRESENT, NULL }, 
                                        { NOT_PRESENT, NULL },
                                        { NOT_PRESENT, NULL }
                                       };

// Vibration pattern when timer expires
const VibePattern timer_done_vibe = {
  .durations = (uint32_t []) {75, 200, 75, 200, 75, 500,
                              75, 200, 75, 200, 75, 500,
                              75, 200, 75, 200, 75, 200},
  .num_segments = 18
};

// Vibration pattern when 5 or 2 mins left.
const VibePattern min52_vibe = {
  .durations = (uint32_t []) {100,200,100,200},
  .num_segments = 4,
};


// Timer modes are editing seconds, editing minutes and running timer. Select button changes modes
// via both short and long clicks.
// Start in MODE_EDIT_SEC (editing seconds)
typedef enum { MODE_EDIT_SEC, MODE_EDIT_MIN, MODE_RUN } Modes;
Modes current_mode = MODE_RUN;

// Timer defaults to 1 minute at app startup
typedef struct {
    int min;
    int sec;
} CounterData;

CounterData init_val = { 10, 0 };        // Remember counter starting point
CounterData curr_val = { 10, 0 };        // Counter current value

typedef struct {
  int yday;
  int wday;
  uint8_t secCount;
  uint8_t taskCount;
} RegisterData;

RegisterData dailyRegisters[7];

int per_last_yday = 0;
int per_last_wday = 0;

int yday = 0;
int wday = 0;

//uint8_t i;
//for(i = 0; i < 6; i++) {
//    dailyRegisters[i] = {i,0,0};        //make registers starting with zero task and section count
//    }

bool timer_running = false;             // Is the timer currently running?
int seconds = 0;                        // Number of seconds in countdown

uint8_t task_count = 0;
uint8_t sec_count = 0;

static int count = 0;                    // Counter in times up text
static bool fullstop = true;


// Layers to display minutes and seconds, a label for minutes and seconds and a time's up message
TextLayer *text_min_layer;               // Layer for displaying minutes
TextLayer *text_sec_layer;               // Layer for displaying seconds
TextLayer *text_label_layer;             // Layer for displaying 'm' and 's' labels
TextLayer *text_times_up_layer;          // Layer for displaying "Time's Up" message
TextLayer *status_layer;                 // Layer for displaying date/time

// Convert integers where 0 <= val <= 59 to two digit text
// For now, change to '!!' if val is out-of-bounds. Will look strange on the Pebble.
void itoa ( int val, char *txt ) {

  if (sizeof(txt) < 2) {
    return;
  }

  if ( ( val >= 0 ) && ( val <= 59 ) ) {
    txt[0] = (char) ('0' + (val / 10));
    txt[1] = (char) ('0' + (val % 10));
  } else {
    txt[0] = '!';
    txt[1] = '!';
  }

}

void update_count() {
  static char count_text[] = "XX";
  itoa(count, count_text);
  text_layer_set_text(text_times_up_layer, count_text);
}

//static void load_curr_chart() {
//  
//  chart_layer_set_plot_type(chart_layer, eLINE);
//  const int x[] = {0,1,2,3,4,5,6};
  
//  int y[7];
//  int z[7];
//  int shiftPos = 6-per_last_wday;
//  for (int i=0; i<7; i++){
    //shift the orther so that the very last record is always at the last.
//    if (i<=per_last_wday){
//      y[i+shiftPos] = (int) dailyRegisters[i].taskCount;
//      z[i+shiftPos] = (int) dailyRegisters[i].secCount;
//    }  else {
//      y[i-per_last_wday-1] = (int) dailyRegisters[i].taskCount;
//      z[i-per_last_wday-1] = (int) dailyRegisters[i].secCount;
//    }
//  }

//  APP_LOG(APP_LOG_LEVEL_DEBUG, "setting data");
//  chart_layer_set_data(chart_layer, x, eINT, y, eINT, z, eINT, 7);
//  chart_layer_set_ymin(chart_layer, 0);
//  APP_LOG(APP_LOG_LEVEL_DEBUG, "after setting data");
// for (unsigned int i = 0; i < data->iPointsToDraw; ++i){
//    graphics_fill_rect(ctx,
//			   ((GRect) {
//			     .origin = { data->pXData[i] - (data->iBarWidth / 2), data->pYData[i] },
//			       .size = { data->iBarWidth, (((data->iYAxisIntercept > (bounds.size.h - data->iMargin)) ? (bounds.size.h - data->iMargin) : data->iYAxisIntercept) - data->pYData[i]) } }),
//			   0,
//			   GCornersAll);
// }
  
//  text_layer_set_text(title_text_layer, "Before        -3          Now");
  
//}




// Redisplay the seconds in the timer. Remember what was there so that the drawing only
// takes place if the seconds have changed since the last time this was called.
void redisplay_sec () {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:redisplay_sec()");
  static char sec_text[] = "XX";
  static int last_sec = -1;
  if (curr_val.sec != last_sec) {
    itoa(curr_val.sec, sec_text);
    last_sec = curr_val.sec;
    text_layer_set_text(text_sec_layer, sec_text);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:redisplay_sec()");

}

// Redisplay the minutes in the timer. Remember what was there so that the drawing only
// takes place if the minutes have changed since the last time this was called
void redisplay_min () {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:redisplay_min()");
  static char min_text[] = "XX";
  static int last_min = -1;
  if (curr_val.min != last_min) {
    itoa(curr_val.min, min_text);
    last_min = curr_val.min;
    text_layer_set_text(text_min_layer, min_text);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:redisplay_min()"); 
}

// Redisplay the timer minutes and seconds because a time tick has occurred while
// the timer is running, or the minutes or seconds have been edited
void redisplay_timer () {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:redisplay_timer()");
  redisplay_min();
  redisplay_sec();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:redisplay_timer()");
}

// Removes the image next to a button, if one is present
void remove_button( ButtonId button_id ) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:remove_button(), button:%2d", button_id);
  if (button_data[button_id].state == PRESENT) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Removing button: %2d", button_id);
    action_bar_layer_clear_icon(action_bar_layer, button_id);
    gbitmap_destroy(button_data[button_id].bitmap);
    button_data[button_id].bitmap = NULL;
    button_data[button_id].state = NOT_PRESENT;
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "No button: %2d to remove", button_id);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:remove_button()");
  
}

// Displays an image next to a button. If there is an existing image, remove it first
void display_button ( ButtonId button_id, uint32_t res_id ) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:display_button()");
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Adding button: %2d", button_id);
  remove_button(button_id);
  button_data[button_id].state = PRESENT;
  button_data[button_id].bitmap = gbitmap_create_with_resource(res_id);
  action_bar_layer_set_icon(action_bar_layer, button_id, button_data[button_id].bitmap);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "display_button: button_id:%2d, bitmap:%p, layer:%p",
          button_id, button_data[button_id].bitmap, action_bar_layer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:display_button()");
}


// Handle a press of the up button.
static void up_single_click_handler (ClickRecognizerRef recognizer, void *ctx) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:up_single_click_handler()");
  switch (current_mode) {
    case MODE_EDIT_SEC:
      // Increment seconds, wrap to 0 after 59
      init_val.sec = init_val.sec == 59 ? 0 : init_val.sec + 1;
      curr_val.sec = init_val.sec;
      redisplay_sec();
      break;
    case MODE_EDIT_MIN:
      // Increment minutes, wrap to 0 after 59
      init_val.min = init_val.min == 55 ? 0 : init_val.min + 5;
      curr_val.min = init_val.min;
      redisplay_min();
      break;
    case MODE_RUN:
      if (!timer_running) {                                          // Start the timer
        seconds = (curr_val.min * 60) + curr_val.sec;
        if (fullstop) {
          count = 0;                                                 // Set count as 0
          update_count();
          
          //read the data from the persist, first rechieve last wDate, then use it to rechieve ydate, 
          //task count and secction count.
          if (persist_exists(PERSIST_LAST_WDATE)){
            per_last_wday = persist_read_int(PERSIST_LAST_WDATE);
          }
  
          if (persist_exists(PERSIST_DATA)){
            persist_read_data(PERSIST_DATA, dailyRegisters, sizeof(dailyRegisters));
          }
      
          task_count =  dailyRegisters[per_last_wday].taskCount;
          sec_count = dailyRegisters[per_last_wday].secCount;
          per_last_yday = dailyRegisters[per_last_wday].yday;
  
          time_t newTime = time(NULL);
	        struct tm *now = localtime(&newTime);
          yday = now->tm_yday;
          wday = now->tm_wday;
         
          if (per_last_yday != yday) {    
            //set the task and section count as zero.
            task_count = 0;
            sec_count = 0;
     
            int day_gap = 1;
    
            while ( (yday - day_gap) > per_last_yday && day_gap <= 6){
              int wday_pointer = (wday - day_gap +7 ) % 7 ;
              dailyRegisters[wday_pointer].wday = wday_pointer;
              dailyRegisters[wday_pointer].yday = yday-day_gap;
              dailyRegisters[wday_pointer].taskCount = 0 ;
              dailyRegisters[wday_pointer].secCount = 0 ;
              day_gap++;
             }
            }
              persist_write_int(PERSIST_LAST_WDATE, wday);
          } 
        
        
          if (seconds != 0) {
            fullstop = false;
            display_button(BUTTON_ID_UP, RESOURCE_ID_PAUSE_IMAGE);
            remove_button(BUTTON_ID_SELECT);
            remove_button(BUTTON_ID_DOWN);
            display_button(BUTTON_ID_SELECT, RESOURCE_ID_MINUS_IMAGE);
            display_button(BUTTON_ID_DOWN, RESOURCE_ID_PLUS_IMAGE);
            seconds = (curr_val.min * 60) + curr_val.sec;       
          }         
          timer_running = (seconds != 0);                  
        } else {                                                       // Pause the timer
          timer_running = false;
          fullstop = false;
          display_button(BUTTON_ID_UP, RESOURCE_ID_START_IMAGE);
        }
    
      break;
      default:
      break;
  } // end switch
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:up_single_click_handler()");

}

static void up_long_click_handler (ClickRecognizerRef recognizer, void *ctx) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:up_long_click_handler()");
  switch (current_mode) {
  
    case MODE_EDIT_SEC:
      break;
    case MODE_EDIT_MIN:
      break;
    case MODE_RUN:
          timer_running = false;
          fullstop = true;
          curr_val.sec = init_val.sec;                                
          curr_val.min = init_val.min;
          display_button(BUTTON_ID_UP, RESOURCE_ID_START_IMAGE);
          redisplay_timer();
          count = 0;                                                 // Set count as 0
          update_count();
    
      break;
    default:
      break;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:up_long_click_handler()");
  
}

static void select_single_click_handler (ClickRecognizerRef recognizer, void *ctx) {
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:select_single_click_handler()");
  switch (current_mode) {
    case MODE_EDIT_SEC:
      // Change to editing minutes, unhighlight seconds, highlight minutes
      current_mode = MODE_EDIT_MIN;
      text_layer_set_text_color(text_sec_layer, GColorWhite);
      text_layer_set_background_color(text_sec_layer, GColorBlack);
      text_layer_set_text_color(text_min_layer, GColorBlack);
      text_layer_set_background_color(text_min_layer, GColorWhite);      
      break;
    case MODE_EDIT_MIN:
      // Change to editing seconds, highlight seconds, unhighlight minutes
      current_mode = MODE_EDIT_SEC;
      text_layer_set_text_color(text_sec_layer, GColorBlack);
      text_layer_set_background_color(text_sec_layer, GColorWhite);
      text_layer_set_text_color(text_min_layer, GColorWhite);
      text_layer_set_background_color(text_min_layer, GColorBlack);
      break;
    case MODE_RUN:
      count--;                                                          //Count down
      update_count();
      break;
    default:
      break;
  } // end switch
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:select_single_click_handler()");

}

static void select_long_click_handler (ClickRecognizerRef recognizer, void *ctx) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:select_long_click_handler()");
  switch (current_mode) {
  
    case MODE_EDIT_SEC:
      current_mode = MODE_RUN;
      text_layer_set_text_color(text_sec_layer, GColorWhite);
      text_layer_set_background_color(text_sec_layer, GColorBlack);
      display_button(BUTTON_ID_UP, RESOURCE_ID_START_IMAGE);
      display_button(BUTTON_ID_DOWN, RESOURCE_ID_RESET_IMAGE);
      break;
    case MODE_EDIT_MIN:
      current_mode = MODE_RUN;
      text_layer_set_text_color(text_min_layer, GColorWhite);
      text_layer_set_background_color(text_min_layer, GColorBlack);
      display_button(BUTTON_ID_UP, RESOURCE_ID_START_IMAGE);
      display_button(BUTTON_ID_DOWN, RESOURCE_ID_RESET_IMAGE);
      redisplay_timer();
      break;
    case MODE_RUN:
      if (!timer_running) {
        current_mode = MODE_EDIT_MIN;
        // text_layer_set_background_color(text_times_up_layer, GColorBlack); // Clear "Time's Up"
        text_layer_set_text_color(text_min_layer, GColorBlack);
        text_layer_set_background_color(text_min_layer, GColorWhite);
        curr_val.sec = init_val.sec;
        curr_val.min = init_val.min;
        display_button(BUTTON_ID_UP, RESOURCE_ID_PLUS_IMAGE);
        display_button(BUTTON_ID_SELECT, RESOURCE_ID_MODE_IMAGE);
        display_button(BUTTON_ID_DOWN, RESOURCE_ID_MINUS_IMAGE);
        redisplay_timer();
      }
      break;
    default:
      break;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:select_long_click_handler()");
  
}

// Handle a press of the down button
static void down_single_click_handler (ClickRecognizerRef recognizer, void *ctx) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:down_single_click_handler()");
  // text_layer_set_background_color(text_times_up_layer, GColorBlack); // Clear "Time's Up"
  switch (current_mode) {
    case MODE_EDIT_SEC:
      // Decrement seconds, wrap to 59 after 0
      init_val.sec = init_val.sec == 0 ? 59 : init_val.sec - 1;
      curr_val.sec = init_val.sec;
      redisplay_sec();
      break;
    case MODE_EDIT_MIN:
      // Decrement minutes, wrap to 59 after 0
      init_val.min = init_val.min == 0 ? 55 : init_val.min - 5;
      curr_val.min = init_val.min;
      redisplay_min();
      break;
    case MODE_RUN:
      //if (!timer_running) {                                   // Reset the timer to the start value
      // curr_val.sec = init_val.sec;
      // curr_val.min = init_val.min;
      // if ( ( curr_val.sec + curr_val.min ) != 0 ) {         // Only display start button if non-zero
      //  display_button(BUTTON_ID_UP, RESOURCE_ID_START_IMAGE);
      //  display_button(BUTTON_ID_SELECT, RESOURCE_ID_MODE_IMAGE);
      //}
      //redisplay_timer();
  //} else {                                                // Timer is running, no action
  //  }
      count++;                                              // Count up
      update_count();
      break;
    default:
      break;
  } // end switch
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:down_single_click_handler()");

}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(text_layer, "Task/Section Trend");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

//  chart_layer = chart_layer_create((GRect) { 
//      .origin = { 0, 40},
//	.size = { bounds.size.w, 80 } });
//  chart_layer_set_plot_color(chart_layer, GColorBlack);
//  chart_layer_set_canvas_color(chart_layer, GColorWhite);
//  chart_layer_show_points_on_line(chart_layer, true);
  //chart_layer_animate(chart_layer, false);
//  layer_add_child(window_layer, chart_layer_get_layer(chart_layer));

//  title_text_layer = text_layer_create((GRect) { .origin = { 0, 125 }, .size = { //bounds.size.w, 20 } });
//  text_layer_set_text_alignment(title_text_layer, GTextAlignmentCenter);
//  layer_add_child(window_layer, text_layer_get_layer(title_text_layer));
//
//  load_curr_chart();
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer);
//  chart_layer_destroy(chart_layer);
  text_layer_destroy(title_text_layer);
}

static void down_long_click_handler (ClickRecognizerRef recognizer, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:select_long_click_handler()");
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true);
}

// Set up button click handlers

static void set_click_config_provider(void *ctx) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:set_click_config_provider()");
  if (button_data[BUTTON_ID_UP].state == PRESENT) {
    window_single_click_subscribe(BUTTON_ID_UP, up_single_click_handler);
    window_long_click_subscribe(BUTTON_ID_UP, 500, up_long_click_handler, NULL);
  } else {
    window_single_click_subscribe(BUTTON_ID_UP, NULL);
    window_long_click_subscribe(BUTTON_ID_UP, 0, NULL, NULL);
  }
  if (button_data[BUTTON_ID_SELECT].state == PRESENT) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_single_click_handler);
    window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);
  } else {
    window_single_click_subscribe(BUTTON_ID_SELECT, NULL);
    window_long_click_subscribe(BUTTON_ID_SELECT, 0, NULL, NULL);
  }
  if (button_data[BUTTON_ID_DOWN].state == PRESENT) {
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, down_single_click_handler);
    window_long_click_subscribe(BUTTON_ID_DOWN, 500, down_long_click_handler, NULL);
  } else {
    window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 0, NULL);
    window_long_click_subscribe(BUTTON_ID_DOWN, 0, NULL, NULL);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:set_click_config_provider()");
  
}


// Decrement the timer, return true if we hit zero
bool decrement_timer () {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:decrement_timer()");
  // Decrement the timer, if at zero then return true, otherwise return false
  // Don't do anything if we are sent a negative number, and set the seconds to zero to stop the timer
  if (seconds > 0 ) {
    seconds--;
    curr_val.min = seconds / 60;
    curr_val.sec = seconds % 60;
    redisplay_timer();
    //Vibration when 5 mins left.
    if (seconds == 300) {
      vibes_enqueue_custom_pattern(min52_vibe);
      
      }
    //Vibration when 2 mins left.
    if (seconds == 120) {
      vibes_enqueue_custom_pattern(min52_vibe);
      
      }
  } else {
    seconds = 0;
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:decrement_timer()");
  return (seconds == 0);

}

// Decrement the timer every second. When we hit zero we notify the user, both visually
// and with a vibration pattern
// TODO: Can this handler be disabled if the timer isn't running???
    
static void handle_second_tick(struct tm *t, TimeUnits units_changed) {  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:handle_second_tick()");
  // Get out of here quick if the timer isn't running

  int hour = t->tm_hour; //---get the hour---
  int minute = t->tm_min; //---get the minute---
  int month = t->tm_mon+1;
  int day = t->tm_mday;


  //---convert to string---
  snprintf(str_hour, sizeof(str_hour), "%02d", hour);
  snprintf(str_minute, sizeof(str_minute), "%02d", minute);
  snprintf(str_month, sizeof(str_month), "%02d", month);
  snprintf(str_day, sizeof(str_day), "%02d", day);
  //---form the string to display the current time---
  strcpy(str_current_status, str_month);
  strcat(str_current_status, "/");
  strcat(str_current_status, str_day);
  strcat(str_current_status, "   ");
  strcat(str_current_status, str_hour);
  strcat(str_current_status, ":");
  strcat(str_current_status, str_minute);
    
  //---display the current hour:minute in a TextLayer---
  text_layer_set_text(status_layer, str_current_status);

  if (timer_running) {
    if (decrement_timer()) {
    
      // Time is up, change the background on the "Time's Up" layer to display the message.
      // Redisplay the '<- and reset images, remove the start image
      // Queue up the vibration notification
      // text_layer_set_background_color(text_times_up_layer, GColorWhite);

      timer_running = false;
      fullstop = true;
      curr_val.sec = init_val.sec;                                
      curr_val.min = init_val.min;
      redisplay_timer(); 
      remove_button(BUTTON_ID_UP);
      display_button(BUTTON_ID_UP, RESOURCE_ID_START_IMAGE);
      vibes_enqueue_custom_pattern(timer_done_vibe);
      
      task_count = task_count + count;
      sec_count = sec_count+1;
      
      dailyRegisters[per_last_wday].yday = yday;
      dailyRegisters[per_last_wday].wday = wday;
      dailyRegisters[per_last_wday].taskCount = task_count; 
      dailyRegisters[per_last_wday].secCount = sec_count; 
      
      APP_LOG(APP_LOG_LEVEL_DEBUG, "yday: %d", yday);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "wday: %d", wday);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "yday: %d", yday);
      APP_LOG(APP_LOG_LEVEL_DEBUG, "yday: %d", yday);
      persist_write_data(PERSIST_DATA, dailyRegisters, sizeof(dailyRegisters));
      
      static char buffer[20];
      snprintf(buffer, sizeof(buffer), "sec %d\ntsk %d", sec_count, task_count);
      text_layer_set_text(text_times_up_layer, buffer);

    }
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:handle_second_tick()");

}


// Initialization
void handle_init(void) {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:handle_init()");
  
  window = window_create();
  Layer *window_layer = window_get_root_layer(window);
  window_set_background_color(window, GColorWhite);
  
  action_bar_layer = action_bar_layer_create();
  action_bar_layer_set_background_color(action_bar_layer, GColorWhite);
  action_bar_layer_add_to_window(action_bar_layer, window);
  action_bar_layer_set_click_config_provider(action_bar_layer, set_click_config_provider);  
  
  GFont timer_font = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GFont label_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont status_font = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
  // Initialize space where minutes are shown
  text_min_layer = text_layer_create(GRect(9, 28, 50, 46));
  text_layer_set_text_color(text_min_layer, GColorBlack);
  text_layer_set_background_color(text_min_layer, GColorWhite);
  text_layer_set_font(text_min_layer, timer_font);
  text_layer_set_text_alignment(text_min_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window_layer, (Layer *)text_min_layer);

  // Initialize space where seconds are shown
  text_sec_layer = text_layer_create(GRect(64, 28, 50, 46));
  text_layer_set_text_color(text_sec_layer, GColorBlack);
  text_layer_set_background_color(text_sec_layer, GColorWhite);
  text_layer_set_font(text_sec_layer, timer_font);
  text_layer_set_text_alignment(text_sec_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window_layer, (Layer *)text_sec_layer);
  
  // Initialize space where the 'm' and 's' labels are shown
  text_label_layer = text_layer_create(GRect(2, 76, 115, 18));
  text_layer_set_text_color(text_label_layer, GColorWhite);
  text_layer_set_background_color(text_label_layer, GColorPurple);
  text_layer_set_font(text_label_layer, label_font);
  layer_add_child((Layer *)window_layer, (Layer *)text_label_layer);
  int charge_percent = battery_state_service_peek().charge_percent;
  snprintf(str_cp, sizeof(str_cp), "%02d", charge_percent);
  strcpy(str_center_status, "        m          s    ");
  strcat(str_center_status, str_cp);
  strcat(str_center_status, "%" );
  text_layer_set_text(text_label_layer, str_center_status);
  
  // Initialize space where the "Time's Up!" message is shown. Displaying
  // is a simple matter of changing the background from black to white
  // so that the text can appear.
  text_times_up_layer = text_layer_create(GRect(9, 95, 108, 64));
  text_layer_set_text_color(text_times_up_layer, GColorBlack);
  text_layer_set_background_color(text_times_up_layer, GColorWhite);
  text_layer_set_font(text_times_up_layer, timer_font);
  text_layer_set_text_alignment(text_times_up_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window_layer, (Layer *)text_times_up_layer);
  text_layer_set_text(text_times_up_layer, "Play,\n Score!");
  
    
  status_layer = text_layer_create(GRect(0, 0, 144, 21));
  text_layer_set_text_color(status_layer, GColorWhite);
  text_layer_set_background_color(status_layer, GColorPurple);
  text_layer_set_font(status_layer, status_font);
  text_layer_set_text_alignment(status_layer, GTextAlignmentCenter);
  layer_add_child((Layer *)window_layer, (Layer *)status_layer);
  text_layer_set_text(status_layer, "updating");
  // Display initial button images. Since we start up editing seconds, we need
  // '+', '<-' and '-'
  display_button(BUTTON_ID_UP, RESOURCE_ID_START_IMAGE);
  display_button(BUTTON_ID_SELECT, RESOURCE_ID_MODE_IMAGE);
  display_button(BUTTON_ID_DOWN, RESOURCE_ID_RESET_IMAGE);
  redisplay_timer();

  // Subscribe to timer ticks every second
  tick_timer_service_subscribe(SECOND_UNIT, handle_second_tick);

  window_stack_push(window, true /* Animated */);
  
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:handle_init()");
  
}

// We have to deinit any BmpContainers that exist when we exit.
void handle_deinit() {

  APP_LOG(APP_LOG_LEVEL_DEBUG, "enter:handle_deinit()");
   

  for (ButtonId i = BUTTON_ID_BACK; i < NUM_BUTTONS; i++) {
    remove_button(i);
  }
  
  action_bar_layer_destroy(action_bar_layer);
  text_layer_destroy(text_min_layer);
  text_layer_destroy(text_sec_layer);
  text_layer_destroy(text_times_up_layer);
  text_layer_destroy(text_label_layer);
  text_layer_destroy(status_layer);
  tick_timer_service_unsubscribe();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "exit:handle_deinit()");
  
}

int main(void) {
  handle_init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Calling app_event_loop");
  app_event_loop();
  handle_deinit();
}