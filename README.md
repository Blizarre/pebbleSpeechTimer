# Pebble Speech Timer

A Very simple app that leverage the Speech-to-Text feature of the pebble to provide a single timer. It is pretty limited right now but it provide s90% of the features I expect from a timer, so it is already useful.

Given the unreliability of the Speech-to-Text, I decided to keep it _very simple_: It will only accept a number, and when that number is processed
it will start a timer of that number of minutes. Maybe it is my accent, but I have found that asking anything beyond simple numbers is not reliable enough for now.

Actions:
- Up button: Restart the timer (can be triggered during or after a timer has been set)
- Center button: Start a new timer
- Down button: Cancel the timer

# How to use

The app starts directly in Speech-to-text mode. Just say the number of minutes that you would like to wait for, and press the center button to trigger the speech-to-text translation.

If the app can recognize the number, it will start waiting automatically. If it cannot it will vibrate once. Press the center button to retry.

The app will display the end time as well as the countdown of the number of minutes remaining.

The watch will vibrate twice at the end of the countdown to alert the user.
