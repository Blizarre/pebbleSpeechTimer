#include "state.h"
#include <pebble.h>
#include <time.h>

static Window *s_window;
static TextLayer *s_main_layer, *s_countdown_layer;
static DictationSession *s_dictation_session;

static char s_main_text[256];
static char s_countdown_text[256];
static State state;

#define STATE_STORAGE_KEY 1

static void stop_timer(char *main_text, char *countdown_text) {
  stop(&state);
  save(&state, STATE_STORAGE_KEY);
  if (main_text != NULL) {
    text_layer_set_text(s_main_layer, main_text);
  }
  if (countdown_text != NULL) {
    text_layer_set_text(s_countdown_layer, countdown_text);
  }
}

static void timer_countdown_callback(int remaining_minutes) {
  time_t end_time = get_end_time(&state);
  if (remaining_minutes > 0.0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Timer keep going for %d minutes",
            remaining_minutes);
    snprintf(s_countdown_text, sizeof(s_countdown_text), "%d minutes",
             (int)remaining_minutes);
    strftime(s_main_text, sizeof(s_main_text), "Wait until %T",
             localtime(&end_time));
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "Timer ended");
    snprintf(s_countdown_text, sizeof(s_countdown_text), "--");
    strftime(s_main_text, sizeof(s_main_text), "Timer ended at %T",
             localtime(&end_time));

    // TODO: Need to pulse more. Another 100ms callback to do it again?
    vibes_double_pulse();
  }
  text_layer_set_text(s_countdown_layer, s_countdown_text);
  text_layer_set_text(s_main_layer, s_main_text);
}

// Start a timer with minutes_in_future. Will Update the UI.
// Restart the existing timer if minutes_in_future is 0
static void start_timer(int minutes_in_future) {
  if (minutes_in_future == 0) {
    minutes_in_future = get_number_of_minutes(&state);
  }
  stop_timer(NULL, NULL);
  start(&state, minutes_in_future);
  save(&state, STATE_STORAGE_KEY);
}

static void handle_dictation_response(DictationSessionStatus status) {
  switch (status) {
  case DictationSessionStatusSuccess:
    // pass
    return;
  case DictationSessionStatusFailureTranscriptionRejected:
  case DictationSessionStatusFailureTranscriptionRejectedWithError:
    text_layer_set_text(s_countdown_layer, "Rejected by user");
    break;
  case DictationSessionStatusFailureSystemAborted:
    text_layer_set_text(s_countdown_layer, "Too many errors");
    break;
  case DictationSessionStatusFailureNoSpeechDetected:
    text_layer_set_text(s_countdown_layer, "No speech detected");
    break;
  case DictationSessionStatusFailureConnectivityError:
    text_layer_set_text(s_countdown_layer, "Connectivity error");
    break;
  case DictationSessionStatusFailureDisabled:
    text_layer_set_text(s_countdown_layer, "Transcription disabled");
    break;
  case DictationSessionStatusFailureInternalError:
    text_layer_set_text(s_countdown_layer, "Internal error");
    break;
  case DictationSessionStatusFailureRecognizerError:
    text_layer_set_text(s_countdown_layer, "Cloud failure");
    break;
  }
}

static void dictation_session_callback(DictationSession *session,
                                       DictationSessionStatus status,
                                       char *transcription, void *context) {
  if (status == DictationSessionStatusSuccess) {
    // Reset the current timers if they were running
    stop_timer(NULL, "--");
    APP_LOG(APP_LOG_LEVEL_INFO, "Transcribed '%s'", transcription);
    int minutes = atoi(transcription);
    if (minutes == 0) {
      // Sometimes the speech-to-text write down the numbers as words so we try
      // the most common options
      if (strcmp(transcription, "One.") == 0) {
        minutes = 1;
      } else if (strncmp(transcription, "Fight", 5) == 0) {
        minutes = 5;
      } else if (strncmp(transcription, "Fun.", 5) == 0) {
        minutes = 5;
      } else if (strncmp(transcription, "Dirty", 5) == 0) {
        minutes = 30;
      }
    }

    if (minutes != 0) {
      start_timer(minutes);
    } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Processed it as 0");
      snprintf(s_main_text, sizeof(s_main_text), "Can't process:\n%s",
               transcription);
      vibes_long_pulse();
      text_layer_set_text(s_main_layer, s_main_text);
    }
  } else {
    handle_dictation_response(status);
  }
}

static void prv_select_click_handler(ClickRecognizerRef _recognizer,
                                     void *_context) {
  handle_dictation_response(dictation_session_start(s_dictation_session));
}

static void prv_up_click_handler(ClickRecognizerRef _recognizer,
                                 void *_context) {
  start_timer(0);
}

static void prv_down_click_handler(ClickRecognizerRef _recognizer,
                                   void *_context) {
  stop_timer("Cancelled", "--");
}

static void prv_click_config_provider(void *_context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void prv_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Window load");
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_main_layer = text_layer_create(GRect(0, 72, bounds.size.w, 40));
  text_layer_set_text(s_main_layer, "Press center button to start");
  text_layer_set_text_alignment(s_main_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_main_layer));

  s_countdown_layer = text_layer_create(GRect(0, 120, bounds.size.w, 20));
  text_layer_set_text(s_countdown_layer, "--");
  text_layer_set_text_alignment(s_countdown_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_countdown_layer));

  s_dictation_session = dictation_session_create(
      sizeof(s_main_text), dictation_session_callback, NULL);
  dictation_session_enable_confirmation(s_dictation_session, false);

  // Now we reload whatever was in storage or create a new empty state
  load_or_new(&state, STATE_STORAGE_KEY, timer_countdown_callback);

  // 2. There is a timer running, it has expired and the app has been waken up
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    if (resume(&state)) {
      // Maybe we have been awaken a little bit before the timer was supposed to
      // end
      APP_LOG(APP_LOG_LEVEL_INFO, "Resumed timer on startup");
      return;
    } else {
      // The only reason we would be awaken at that point is if the timer was
      // done And we were awaken a little bit after. So we fire the end of timer
      // event
      timer_countdown_callback(0);
    }
    return;

    // 2. The user started the app themselved
  } else if (launch_reason() == APP_LAUNCH_USER ||
             launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    if (resume(&state)) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Resumed timer on startup");
      return;
    }
    handle_dictation_response(dictation_session_start(s_dictation_session));
    return;
  }

  // 3. We have been started by a system process (like after an install).
  // In that case we just show the default values.
}

static void prv_window_unload(Window *_window) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Window unload");
  text_layer_destroy(s_main_layer);
  dictation_session_destroy(s_dictation_session);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = prv_window_load,
                                           .unload = prv_window_unload,
                                       });
  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) { window_destroy(s_window); }

int main(void) {
  prv_init();
  app_event_loop();
  APP_LOG(APP_LOG_LEVEL_INFO, "Event loop done");
  prv_deinit();
  APP_LOG(APP_LOG_LEVEL_INFO, "De-Init done");
}
