#pragma once

#include <pebble.h>
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
} State;

time_t get_end_time(State *state);
int get_number_of_minutes(State *state);

void load_or_new(State *state, int32_t key, void (*callback_tick)(int));

int save(State *state, int32_t key);

int resume(State *state);

void start(State *state, int number_of_minutes);

void stop(State *state);
