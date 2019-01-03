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

You'll need ncurses, libcurl, iniparser, and jansson installed. If that's the
case, you should just be able to run `make` and end up with a binary called
`cweather`.

## Configuration

cweather reads configuration variables in four ways. In descending order of
precedence these are command line arguments, environment variables, config
file options read from `~/.cweather` (see
[cweather_example](./cweather_example)), and any compiled in defaults.

The configurable values right now are your location, and the update interval.

| Name | Default | Config File Option | Environment Variable | Argument |
| Location | `-37.8136,144.9631` | location | LOCATION | -l |
| Interval | `300` | interval | INTERVAL | -i |

## Running

Once the program is running, you can press `q` to quit, or `u` to force an
update.

## License

GPLv3
