# Pebble Speech Timer

A Very simple app that leverage the Speech-to-Text feature of the pebble to provide a single timer.

Given the unreliability of the Speech-to-Text, I decided to keep it _very simple_: It will only accept a number, and when that number is processed
it will start a timer of that number of minutes.

# How to use

The app starts directly in Speech-to-text mode.

It is recommended to assign this app to the center button shortcut. A long press on the shortcut will start the app in speech-to-text mode.
Just say the number of minutes (just the number), and press the center button to start the processing.

You can stop looking at the watch. A single vibration means that the watch didn't understand the number, nothing means that the countdown has started.
The app will display the end time as well as the countdown of the number of minutes remaining.

The watch will vibrate twice at the end of the countdown to alert the user.

It sounds pretty limited but it is actually exactly what I want 95% of the time.
