#include <pebble.h>
#include <time.h>

static Window *s_window;
static TextLayer *s_message_layer, *s_countdown_layer;
static DictationSession *s_dictation_session;
static AppTimer *s_timer_end_handle = NULL;
static AppTimer *s_timer_countdown_handle = NULL;
int waiting = 0;

static char s_last_text[512];
static char s_countdown_text[512];

static void timer_callback(void* data) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Timer ended");
  text_layer_set_text(s_message_layer, "Done");

  if (s_timer_countdown_handle != NULL) {
    app_timer_cancel(s_timer_countdown_handle);
    s_timer_countdown_handle = NULL;
  }
  text_layer_set_text(s_countdown_layer, "--");

  vibes_double_pulse();
}

static void cancel_all() {
  if (s_timer_end_handle != NULL) {
    app_timer_cancel(s_timer_end_handle);
    s_timer_end_handle = NULL;
    text_layer_set_text(s_message_layer, "Canceled");
  }
  if (s_timer_countdown_handle != NULL) {
    app_timer_cancel(s_timer_countdown_handle);
    s_timer_countdown_handle = NULL;
    text_layer_set_text(s_countdown_layer, "--");
  }
}

static void countdown_callback(void* data) {
  int* remaining = (int*)data;
  *remaining -= 1;
  if (*remaining > 0) {
    s_timer_countdown_handle = app_timer_register(1000 * 60, countdown_callback, data);
  }
  snprintf(s_countdown_text, sizeof(s_countdown_text), "%d minutes", *remaining);
  text_layer_set_text(s_countdown_layer, s_countdown_text);
}


static void dictation_session_callback(DictationSession *session, DictationSessionStatus status,
                                       char *transcription, void *context) {
  // Reset the current timer if it was running
  if (status == DictationSessionStatusSuccess) {
    cancel_all();
    APP_LOG(APP_LOG_LEVEL_INFO, "Transcribed '%s'", transcription);
    waiting = atoi(transcription);
    if (waiting == 0) {
      // Sometimes the speech-to-text write down the numbers as words so we try
      // the most common options
      if (strcmp(transcription, "one") == 0) {
        waiting = 1;
      } else if (strcmp(transcription, "two") == 0) {
        waiting = 2;
      } else if (strcmp(transcription, "three") == 0) {
        waiting = 3;
      } else if (strcmp(transcription, "four") == 0) {
        waiting = 4;
      } else if (strcmp(transcription, "five") == 0) {
        waiting = 3;
      }
    }

    if (waiting != 0) {
      time_t now;
      time(&now);
      struct tm* tm_value = localtime(&now);
      tm_value->tm_min += waiting;
      time_t after = mktime(tm_value);
      strftime(s_last_text, sizeof(s_countdown_text), "Wait until %T", localtime(&after));

      //snprintf(s_last_text, sizeof(s_last_text), "Waiting %d minutes", waiting);
      snprintf(s_countdown_text, sizeof(s_last_text), "%d minutes", waiting);

      s_timer_end_handle = app_timer_register(waiting * 1000 * 60, timer_callback, &waiting);
      s_timer_countdown_handle = app_timer_register(1000 * 60, countdown_callback, &waiting);
    } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Processed it as 0");
      snprintf(s_last_text, sizeof(s_last_text), "Invalid: %s", transcription);
      vibes_long_pulse();
    }
  } else {
    snprintf(s_last_text, sizeof(s_last_text), "Error %d", (int)status);
  }
  text_layer_set_text(s_countdown_layer, s_countdown_text);
  text_layer_set_text(s_message_layer, s_last_text);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  dictation_session_start(s_dictation_session);  
}

static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(s_message_layer, "Up");
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  cancel_all();
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_message_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
  text_layer_set_text(s_message_layer, "!!");
  text_layer_set_text_alignment(s_message_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_message_layer));

  s_countdown_layer = text_layer_create(GRect(0, 120, bounds.size.w, 20));
  text_layer_set_text(s_countdown_layer, "--");
  text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_layer));


  s_dictation_session = dictation_session_create(sizeof(s_last_text),
                                dictation_session_callback, NULL);
  dictation_session_enable_confirmation(s_dictation_session, false);
  dictation_session_start(s_dictation_session);  
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_message_layer);
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
