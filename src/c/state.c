#include "state.h"
#include "pebble.h"

void tick(State *state);

time_t get_end_time(State *state) { return state->serialized.end_time; }

int get_number_of_minutes(State *state) {
  return state->serialized.timer_number_of_minutes;
}

/// Will load the state if it exists. If not will create a new one
void load_or_new(State *state, int32_t key, void (*callback_tick)(int)) {
  bool reset_state = true;
  if (persist_exists(key)) {
    int result =
        persist_read_data(key, &state->serialized, sizeof(SerializedState));
    if (result == sizeof(SerializedState)) {
      reset_state = false;
    } else {
      APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid state found with size: %d", result);
    }
  } else {
    APP_LOG(APP_LOG_LEVEL_INFO, "No state found");
  }

  if (reset_state) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Resetting state");
    state->serialized.end_time = 0;
    state->serialized.wakeup_id = 0;
  }

  state->callback_tick = callback_tick;
  state->timer_handle = NULL;
}

int save(State *state, int32_t key) {
  int result =
      persist_write_data(key, &state->serialized, sizeof(SerializedState));
  if (result < 0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Error saving state: %d", result);
  }
  return result;
}

/// This is just a wrapper for the real tick with a proper type
void tick_void(void *data) { tick((State *)data); }

void tick(State *state) {
  float diff = difftime(state->serialized.end_time, time(NULL));

  state->callback_tick((int)(diff / 60));

  if (diff <= 0.0) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Ended timer, not renewing (diff %f ms.)",
            diff * 1000);
    return;
  }

  int wait_time_ms;
  if (diff > 60.0) {
    wait_time_ms = 60 * 1000;
  } else {
    wait_time_ms = (int)(diff * 1000);
  }
  state->timer_handle = app_timer_register(wait_time_ms, tick_void, state);
}

/// Try to resume from the state. Return 0 if no valid sessions were found
/// Note: this will not resume if we were just awaken a few ms after the end
/// of the timer.
bool resume(State *state) {
  if (state->serialized.end_time < time(NULL)) {
    // The timer in the state ended in the past, there is nothing for us to do
    return false;
  }
  // We schedule the tick as the next thing to run, it will recover from there
  state->timer_handle = app_timer_register(1, tick_void, state);
  return true;
}

void stop(State *state) {
  if (state->timer_handle != NULL) {
    app_timer_cancel(state->timer_handle);
  }
  if (wakeup_query(state->serialized.wakeup_id, NULL)) {
    wakeup_cancel(state->serialized.wakeup_id);
  }
  state->serialized.end_time = 0;
  state->serialized.wakeup_id = 0;
  state->timer_handle = NULL;
}

static time_t get_time_in_future(int minutes_in_future) {
  time_t now;
  time(&now);
  struct tm *tm_value = localtime(&now);
  tm_value->tm_min += minutes_in_future;
  return mktime(tm_value);
}

void start(State *state, int number_of_minutes) {
  time_t end_time = get_time_in_future(number_of_minutes);
  state->serialized.timer_number_of_minutes = number_of_minutes;
  state->serialized.end_time = end_time;
  WakeupId wakeup_id;
  do {
    wakeup_id = wakeup_schedule(end_time, 0, false);
    if (wakeup_id == E_RANGE) {
      APP_LOG(APP_LOG_LEVEL_ERROR,
              "wakeup inavailable at that time, will try 1 minute earlier");
      end_time -= 60;
    }
  } while (wakeup_id == E_RANGE);
  state->serialized.wakeup_id = wakeup_id;
  state->timer_handle = app_timer_register(1, tick_void, state);
}
