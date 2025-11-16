#pragma once

#include <pebble.h>
#include <stdbool.h>
#include <time.h>

typedef struct _serialized_state {
  time_t timer_number_of_minutes;
  time_t end_time;
  WakeupId wakeup_id;
} SerializedState;

typedef struct _state {
  SerializedState serialized;
  AppTimer *timer_handle;
  void (*callback_tick)(int);
} TimerState;

// Accessors
time_t timer_get_end_time(TimerState *state);
int timer_get_number_of_minutes(TimerState *state);

void timer_load_or_new(TimerState *state, int32_t key,
                       void (*callback_tick)(int));

int timer_save(TimerState *state, int32_t key);

bool timer_resume(TimerState *state);

void timer_start(TimerState *state, int number_of_minutes);

void timer_stop(TimerState *state);
