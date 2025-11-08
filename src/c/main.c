#include <pebble.h>
#include <time.h>

static Window *s_window;
static TextLayer *s_main_layer, *s_countdown_layer;
static DictationSession *s_dictation_session;
static AppTimer *s_timer_end_handle = NULL;
static AppTimer *s_timer_countdown_handle = NULL;
static int timer_minutes_remaining = 0;
static int timer_minutes = 0;

static char s_main_text[256];
static char s_countdown_text[256];

static void timer_main_callback(void* data) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Timer ended");

  if (s_timer_countdown_handle != NULL) {
    app_timer_cancel(s_timer_countdown_handle);
    s_timer_countdown_handle = NULL;
  }

  text_layer_set_text(s_main_layer, "Done");
  text_layer_set_text(s_countdown_layer, "--");

  vibes_double_pulse();
}

static void timer_countdown_callback(void* void_remaining_minutes) {
  int* remaining_minutes = (int*)void_remaining_minutes;
  *remaining_minutes -= 1;
  if (*remaining_minutes > 0) {
    s_timer_countdown_handle = app_timer_register(1000 * 60, timer_countdown_callback, remaining_minutes);
  }
  snprintf(s_countdown_text, sizeof(s_countdown_text), "%d minutes", *remaining_minutes);
  text_layer_set_text(s_countdown_layer, s_countdown_text);
}

static void stop_all_timers(char* main_text, char* countdown_text) {
  if (s_timer_end_handle != NULL) {
    app_timer_cancel(s_timer_end_handle);
    s_timer_end_handle = NULL;
  }
  if (s_timer_countdown_handle != NULL) {
    app_timer_cancel(s_timer_countdown_handle);
    s_timer_countdown_handle = NULL;
  }
  if(main_text != NULL) {
    text_layer_set_text(s_main_layer, main_text);
  }
  if (countdown_text != NULL) {
    text_layer_set_text(s_countdown_layer, countdown_text);
  }
}

static struct tm* get_time_in_future(int minutes_in_future) {
    time_t now;
    time(&now);
    struct tm* tm_value = localtime(&now);
    tm_value->tm_min += minutes_in_future;
    time_t after = mktime(tm_value);
    return localtime(&after);
}

// Start a timer with minutes_in_future. Will Update the UI.
// Restart the existing timer if minutes_in_future is 0
static void start_timer(int minutes_in_future) {
  if (minutes_in_future != 0) {
    timer_minutes = minutes_in_future;
  }
  stop_all_timers(NULL, NULL);
  timer_minutes_remaining = timer_minutes;
  strftime(s_main_text, sizeof(s_countdown_text), "Wait until %T", get_time_in_future(timer_minutes));
  snprintf(s_countdown_text, sizeof(s_main_text), "%d minutes", timer_minutes);

  s_timer_end_handle = app_timer_register(timer_minutes * 1000 * 60, timer_main_callback, &timer_minutes);
  s_timer_countdown_handle = app_timer_register(1000 * 60, timer_countdown_callback, &timer_minutes_remaining);

  text_layer_set_text(s_countdown_layer, s_countdown_text);
  text_layer_set_text(s_main_layer, s_main_text);
}

static void dictation_session_callback(DictationSession *session, DictationSessionStatus status,
                                       char *transcription, void *context) {
  if (status == DictationSessionStatusSuccess) {
    // Reset the current timers if they were running
    stop_all_timers(NULL, "--");
    APP_LOG(APP_LOG_LEVEL_INFO, "Transcribed '%s'", transcription);
    int minutes = atoi(transcription);
    if (minutes == 0) {
      // Sometimes the speech-to-text write down the numbers as words so we try
      // the most common options
      if (strcmp(transcription, "One.") == 0) {
        minutes = 1;
      } else if (strncmp(transcription, "Fight", 5) == 0) {
        minutes = 5;
      }
    }

    if (minutes != 0) {
      start_timer(minutes);
    } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Processed it as 0");
      snprintf(s_main_text, sizeof(s_main_text), "Can't process:\n%s", transcription);
      vibes_long_pulse();
      text_layer_set_text(s_main_layer, s_main_text);
    }
  } else {
    snprintf(s_main_text, sizeof(s_main_text), "Error %d", (int)status);
    text_layer_set_text(s_main_layer, s_main_text);
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  dictation_session_start(s_dictation_session);  
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  start_timer(0);
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  stop_all_timers("Cancelled", "--");
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_main_layer = text_layer_create(GRect(0, 72, bounds.size.w, 40));
  text_layer_set_text(s_main_layer, "!!");
  text_layer_set_text_alignment(s_main_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_main_layer));

  s_countdown_layer = text_layer_create(GRect(0, 120, bounds.size.w, 20));
  text_layer_set_text(s_countdown_layer, "--");
  text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_layer));


  s_dictation_session = dictation_session_create(sizeof(s_main_text),
                                dictation_session_callback, NULL);
  dictation_session_enable_confirmation(s_dictation_session, false);
  dictation_session_start(s_dictation_session);  
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_main_layer);
  dictation_session_destroy(s_dictation_session);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}
