# cweather

This is a text-mode program for showing the weather and a limited forecast.
It's mostly a port of (now defunct) cursetheweather, but written in C instead
of python. Call it an experiment.

This shamelessly hooks into the undocumented API on weather.com - it's even
using the API key from their browser app. I will address this eventually.

This is a bit less finished than I'd like it to be, but I might not get a
chance to polish it up anytime soon so I figured I'd just throw it into the
open. If anyone else feels like making it do more stuff, go nuts.

## Building

You'll need ncurses, libcurl, and jansson installed. If that's the case, you
should just be able to run `make` and end up with a binary called `cweather`.

## Running

You'll probably need to edit the source code to set your location. Even
better, add a text input to search for a place name. Right now it's hardcoded
to search for "Melbourne".

Once the program is running, you can press `q` to quit, or `l` to choose a new
location.

## License

GPLv3
