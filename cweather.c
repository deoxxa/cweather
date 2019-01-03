//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

// _XOPEN_SOURCE is needed for strptime from time.h
#define _XOPEN_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>
#include <iniparser/iniparser.h>
#include <jansson.h>
#include <ncurses.h>

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)

#define CONFIG_FILE ".cweather"
#define DEFAULT_LOCATION "-37.8136,144.9631"
#define DEFAULT_INTERVAL 300
#define MINIMUM_INTERVAL 60

const char ICON_UNKNOWN[] =
    "      -----\n"
    "     '     |\n"
    "         .`\n"
    "        '\n"
    "        .\n";

const char ICON_LIGHTNING[] =
    "    \\\\\\\\\n"
    "    \\\\\\\\\\\\\n"
    "       \\\\\\\\\n"
    "         \\\\\\\n"
    "            \\\n";

const char ICON_CLOUDY[] =
    "     __         _\n"
    "   /-   \\_/\\---/ \\\n"
    "   |         --   |\n"
    "   \\-_/---\\__--__-|\n";

const char ICON_RAINING[] =
    "     __         _\n"
    "   /-   \\_/\\---/ \\\n"
    "   |         --   |\n"
    "   |______________|\n"
    "    . , . , . , .\n";

const char ICON_SUNNY[] =
    "     \\  ___  /\n"
    "      /     \\\n"
    "   - |       | -\n"
    "      \\ ___ / .\n"
    "     / ,  .  \\\n";

const char ICON_CLEAR[] =
    "      ___\n"
    "    /     \\\n"
    "   |       |\n"
    "    \\ ___ /\n";

const char ICON_PARTLYCLOUDY[] =
    "     ` .--- ____\n"
    "    - /....|    \\/ \\\n"
    "    - |...|         |\n"
    "     , \\___\\   __  /\n"
    "      / |   --/  \\\n";

struct icon_s {
  const char *phrase, *day, *night;
};

struct icon_s icons[] = {
    {"Tropical Storm", ICON_LIGHTNING, ICON_LIGHTNING},
    {"Thunder and Hail", ICON_LIGHTNING, ICON_LIGHTNING},
    {"Rain to Snow Showers", ICON_RAINING, ICON_RAINING},
    {"Rain / Sleet", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Wintry Mix Snow / Sleet", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Freezing Drizzle", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Drizzle", ICON_RAINING, ICON_RAINING},
    {"Freezing Rain", ICON_RAINING, ICON_RAINING},
    {"Light Rain", ICON_RAINING, ICON_RAINING},
    {"Rain", ICON_RAINING, ICON_RAINING},
    {"Scattered Flurries", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Light Snow", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Blowing / Drifting Snow", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Snow", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Hail", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Sleet", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Blowing Dust / Sandstorm", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Foggy", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Haze / Windy", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Smoke / Windy", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Blowing Spray / Windy", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Frigid / Ice Crystals", ICON_UNKNOWN, ICON_UNKNOWN},
    {"Cloudy", ICON_CLOUDY, ICON_CLOUDY},
    {"Mostly Cloudy", ICON_PARTLYCLOUDY, ICON_PARTLYCLOUDY},
    {"Partly Cloudy", ICON_PARTLYCLOUDY, ICON_PARTLYCLOUDY},
    {"Clear", ICON_CLEAR, ICON_CLEAR},
    {"Sunny", ICON_SUNNY, ICON_SUNNY},
    {"Fair / Mostly Clear", ICON_CLEAR, ICON_CLEAR},
    {"Fair / Mostly Sunny", ICON_SUNNY, ICON_SUNNY},
    {"Thunderstorms", ICON_LIGHTNING, ICON_LIGHTNING},
    {"Heavy Rain", ICON_RAINING, ICON_RAINING},
    {"Heavy Snow", ICON_RAINING, ICON_RAINING},
    {"Scattered Thunderstorms", ICON_LIGHTNING, ICON_LIGHTNING},
    {NULL, NULL},
};

struct buf_s {
  size_t cap, len;
  char *data;
};

unsigned int buf_write_cb(char *in, unsigned int len, unsigned int nmemb,
                          struct buf_s *buf) {
  unsigned int r;

  r = len * nmemb;

  if (buf->len + r >= buf->cap) {
    return 0;
  }

  memcpy(&(buf->data[buf->len]), in, r);
  buf->len += r;

  return r;
}

int fetch_json(char *url, struct buf_s *buf) {
  CURL *ch;

  ch = curl_easy_init();

  curl_easy_setopt(ch, CURLOPT_URL, url);

  curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, buf_write_cb);
  curl_easy_setopt(ch, CURLOPT_WRITEDATA, buf);
  curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_perform(ch);
  curl_easy_cleanup(ch);

  return 0;
}

struct observation_s {
  int ready;
  char phrase[50];
  int temperature, temperature_min, temperature_max;
  int feels_like;
  int humidity;
  char wind_direction_compass[5];
  int wind_direction_degrees;
  int wind_speed;
  double visibility;
  int uv_index;
  char uv_description[50];
};

int fetch_observation(const char location[],
                      struct observation_s *observation) {
  int rc;
  struct buf_s b;
  char url[250], data[1024 * 32];
  json_t *r, *o;
  json_error_t err;
  char *s1, *s2, *s3;
  size_t l1, l2, l3;

  memset(url, 0, sizeof(url));
  memset(data, 0, sizeof(data));

  b.cap = 1024 * 32;
  b.len = 0;
  b.data = data;

  sprintf(url,
          "https://api.weather.com/v2/turbo/"
          "vt1observation?apiKey=d522aa97197fd864d36b418f39ebb323&"
          "geocode=%s&units=m&language=en-AU&format="
          "json",
          location);

  if ((rc = fetch_json(url, &b)) != 0) {
    return rc;
  }

  r = json_loads(b.data, 0, &err);

  if (!r || !json_is_object(r)) {
    json_decref(r);
    return -1;
  }

  o = json_object_get(r, "vt1observation");
  if (!o || !json_is_object(o)) {
    json_decref(r);
    return -1;
  }

  memset(observation, 0, sizeof(struct observation_s));

  rc = json_unpack(
      o, "{s:s% s:i s:i s:i s:i s:s% s:i s:i s:F s:i s:s%}", "phrase", &s1, &l1,
      "temperature", &observation->temperature, "temperatureMaxSince7am",
      &observation->temperature_max, "feelsLike", &observation->feels_like,
      "humidity", &observation->humidity, "windDirCompass", &s2, &l2,
      "windDirDegrees", &observation->wind_direction_degrees, "windSpeed",
      &observation->wind_speed, "visibility", &observation->visibility,
      "uvIndex", &observation->uv_index, "uvDescription", &s3, &l3);
  if (rc != 0) {
    json_decref(r);
    return -1;
  }

  strncpy(observation->phrase, s1, l1);
  strncpy(observation->wind_direction_compass, s2, l2);
  strncpy(observation->uv_description, s3, l3);

  observation->ready = 1;

  json_decref(r);

  return 0;
}

struct forecast_part_s {
  int valid;
  char day_part_name[20];
  int precip;
  double precip_amount;
  char precip_type[20];
  int temperature;
  int uv_index;
  char uv_description[20];
  int icon;
  int icon_extended;
  char phrase[30];
  char narrative[150];
  int cloud;
  char wind_direction_compass[5];
  int wind_direction_degrees;
  int wind_speed;
  int humidity;
  char qualifier[10];
  char snow_range[10];
  int thunder_enum;
  char thunder_enum_phrase[20];
};

struct forecast_day_s {
  struct tm valid_date, sunrise, sunset, moonrise, moonset;
  char moon_icon[5], moon_phrase[20], weekday[10];
  int snow_qpf;

  struct forecast_part_s day;
  struct forecast_part_s night;
};

struct forecast_s {
  char id[20];
  struct forecast_day_s days[14];
};

#define READ_FORECAST_DAY_S_STRING(O, JN, FN)              \
  e = json_object_get(O, JN);                              \
  if (!e || !json_is_array(e)) {                           \
    fprintf(stderr, JN " was not an array\n");             \
    json_decref(r);                                        \
    return -1;                                             \
  }                                                        \
                                                           \
  n = json_array_size(e);                                  \
                                                           \
  for (i = 0; i < MIN(n, 14); i++) {                       \
    if ((v = json_array_get(e, i)) && json_is_string(v)) { \
      strncpy(forecast->days[i].FN, json_string_value(v),  \
              sizeof(forecast->days[i].FN));               \
    }                                                      \
  }

#define READ_FORECAST_DAY_S_NUMBER(O, JN, FN)              \
  e = json_object_get(O, JN);                              \
  if (!e || !json_is_array(e)) {                           \
    fprintf(stderr, JN " was not an array\n");             \
    json_decref(r);                                        \
    return -1;                                             \
  }                                                        \
                                                           \
  n = json_array_size(e);                                  \
                                                           \
  for (i = 0; i < MIN(n, 14); i++) {                       \
    if ((v = json_array_get(e, i)) && json_is_number(v)) { \
      forecast->days[i].FN = json_number_value(v);         \
    }                                                      \
  }

#define READ_FORECAST_DAY_S_TIME(O, JN, FN)                             \
  e = json_object_get(O, JN);                                           \
  if (!e || !json_is_array(e)) {                                        \
    fprintf(stderr, JN " was not an array\n");                          \
    json_decref(r);                                                     \
    return -1;                                                          \
  }                                                                     \
                                                                        \
  n = json_array_size(e);                                               \
                                                                        \
  for (i = 0; i < MIN(n, 14); i++) {                                    \
    if ((v = json_array_get(e, i)) && json_is_string(v)) {              \
      strptime(json_string_value(v), "%FT%T%z", &forecast->days[i].FN); \
    }                                                                   \
  }

int fetch_forecast(const char location[], struct forecast_s *forecast) {
  int i, rc, n;
  struct buf_s b;
  char url[250], data[1024 * 100];
  json_t *r, *o, *e, *v;
  json_error_t err;

  memset(url, 0, sizeof(url));
  memset(data, 0, sizeof(data));

  b.cap = 1024 * 100;
  b.len = 0;
  b.data = data;

  sprintf(url,
          "https://api.weather.com/v2/turbo/"
          "vt1dailyForecast?apiKey=d522aa97197fd864d36b418f39ebb323&geocode=%s&"
          "units=m&language=en-AU&format=json",
          location);

  if ((rc = fetch_json(url, &b)) != 0) {
    return rc;
  }

  r = json_loads(b.data, 0, &err);

  if (!r || !json_is_object(r)) {
    fprintf(stderr, "payload was not an object\n");
    json_decref(r);
    return -1;
  }

  o = json_object_get(r, "vt1dailyForecast");
  if (!o || !json_is_object(o)) {
    fprintf(stderr, "vt1dailyForecast was not an object\n");
    json_decref(r);
    return -1;
  }

  memset(forecast, 0, sizeof(struct forecast_s));

  READ_FORECAST_DAY_S_TIME(o, "validDate", valid_date)
  READ_FORECAST_DAY_S_STRING(o, "dayOfWeek", weekday)
  READ_FORECAST_DAY_S_TIME(o, "sunrise", sunrise)
  READ_FORECAST_DAY_S_TIME(o, "sunset", sunset)
  READ_FORECAST_DAY_S_STRING(o, "moonIcon", moon_icon)
  READ_FORECAST_DAY_S_STRING(o, "moonPhrase", moon_phrase)
  READ_FORECAST_DAY_S_TIME(o, "moonrise", moonrise)
  READ_FORECAST_DAY_S_TIME(o, "moonset", moonset)

  o = json_object_get(json_object_get(r, "vt1dailyForecast"), "day");
  if (!o || !json_is_object(o)) {
    fprintf(stderr, "vt1dailyForecast.day was not an object\n");
    json_decref(r);
    return -1;
  }

  READ_FORECAST_DAY_S_STRING(o, "dayPartName", day.day_part_name)
  READ_FORECAST_DAY_S_NUMBER(o, "precipPct", day.precip)
  READ_FORECAST_DAY_S_NUMBER(o, "precipAmt", day.precip_amount)
  READ_FORECAST_DAY_S_STRING(o, "precipType", day.precip_type)
  READ_FORECAST_DAY_S_NUMBER(o, "temperature", day.temperature);
  READ_FORECAST_DAY_S_NUMBER(o, "uvIndex", day.uv_index);
  READ_FORECAST_DAY_S_STRING(o, "uvDescription", day.uv_description)
  READ_FORECAST_DAY_S_NUMBER(o, "icon", day.icon);
  READ_FORECAST_DAY_S_NUMBER(o, "iconExtended", day.icon_extended);
  READ_FORECAST_DAY_S_STRING(o, "phrase", day.phrase)
  READ_FORECAST_DAY_S_STRING(o, "narrative", day.narrative)
  READ_FORECAST_DAY_S_NUMBER(o, "cloudPct", day.cloud);
  READ_FORECAST_DAY_S_STRING(o, "windDirCompass", day.wind_direction_compass)
  READ_FORECAST_DAY_S_NUMBER(o, "windDirDegrees", day.wind_direction_degrees);
  READ_FORECAST_DAY_S_NUMBER(o, "windSpeed", day.wind_speed);
  READ_FORECAST_DAY_S_NUMBER(o, "humidityPct", day.humidity);
  READ_FORECAST_DAY_S_STRING(o, "qualifier", day.qualifier)
  READ_FORECAST_DAY_S_STRING(o, "snowRange", day.snow_range)
  READ_FORECAST_DAY_S_NUMBER(o, "thunderEnum", day.thunder_enum);
  READ_FORECAST_DAY_S_STRING(o, "thunderEnumPhrase", day.thunder_enum_phrase)

  o = json_object_get(json_object_get(r, "vt1dailyForecast"), "night");
  if (!o || !json_is_object(o)) {
    fprintf(stderr, "vt1dailyForecast.night was not an object\n");
    json_decref(r);
    return -1;
  }

  READ_FORECAST_DAY_S_STRING(o, "dayPartName", night.day_part_name)
  READ_FORECAST_DAY_S_NUMBER(o, "precipPct", night.precip)
  READ_FORECAST_DAY_S_NUMBER(o, "precipAmt", night.precip_amount)
  READ_FORECAST_DAY_S_STRING(o, "precipType", night.precip_type)
  READ_FORECAST_DAY_S_NUMBER(o, "temperature", night.temperature);
  READ_FORECAST_DAY_S_NUMBER(o, "uvIndex", night.uv_index);
  READ_FORECAST_DAY_S_STRING(o, "uvDescription", night.uv_description)
  READ_FORECAST_DAY_S_NUMBER(o, "icon", night.icon);
  READ_FORECAST_DAY_S_NUMBER(o, "iconExtended", night.icon_extended);
  READ_FORECAST_DAY_S_STRING(o, "phrase", night.phrase)
  READ_FORECAST_DAY_S_STRING(o, "narrative", night.narrative)
  READ_FORECAST_DAY_S_NUMBER(o, "cloudPct", night.cloud);
  READ_FORECAST_DAY_S_STRING(o, "windDirCompass", night.wind_direction_compass)
  READ_FORECAST_DAY_S_NUMBER(o, "windDirDegrees", night.wind_direction_degrees);
  READ_FORECAST_DAY_S_NUMBER(o, "windSpeed", night.wind_speed);
  READ_FORECAST_DAY_S_NUMBER(o, "humidityPct", night.humidity);
  READ_FORECAST_DAY_S_STRING(o, "qualifier", night.qualifier)
  READ_FORECAST_DAY_S_STRING(o, "snowRange", night.snow_range)
  READ_FORECAST_DAY_S_NUMBER(o, "thunderEnum", night.thunder_enum);
  READ_FORECAST_DAY_S_STRING(o, "thunderEnumPhrase", night.thunder_enum_phrase)

  json_decref(r);

  return 0;
}

void update_current(WINDOW *w, struct observation_s *observation, int interval,
                    time_t t) {
  char str[65];
  struct tm lt;
  time_t n;
  int i;

  n = time(NULL);

  localtime_r(&n, &lt);

  // wclear(w);

  if (observation->ready) {
    mvwaddstr(w, 1, 0, ICON_UNKNOWN);

    for (i = 0; icons[i].phrase != NULL; i++) {
      if (strncmp(observation->phrase, icons[i].phrase,
                  sizeof(observation->phrase)) == 0) {
        if (lt.tm_hour > 5 && lt.tm_hour < 19) {
          mvwaddstr(w, 1, 0, icons[i].day);
        } else {
          mvwaddstr(w, 1, 0, icons[i].night);
        }

        break;
      }
    }

    snprintf(str, sizeof(str), "%s", observation->phrase);
    mvwaddstr(w, 7, 2, str);

    strftime(str, sizeof(str), "     Time: %T", &lt);
    mvwaddstr(w, 9, 2, str);

    switch (t) {
      case -1:
        snprintf(str, sizeof(str), "  Updated: error");
        break;
      case 0:
        snprintf(str, sizeof(str), "  Updated: updating");
        break;
      default:
        localtime_r(&t, &lt);
        strftime(str, sizeof(str), "  Updated: %T", &lt);
        break;
    }
    mvwaddstr(w, 10, 2, str);

    snprintf(str, sizeof(str), " Interval: %ds", interval);
    mvwaddstr(w, 11, 2, str);

    snprintf(str, sizeof(str), "Temp/feel: %dc/%dc", observation->temperature,
             observation->feels_like);
    mvwaddstr(w, 12, 2, str);

    snprintf(str, sizeof(str), "  Min/max: %dc/%dc",
             observation->temperature_min, observation->temperature_max);
    mvwaddstr(w, 13, 2, str);

    snprintf(str, sizeof(str), " Humidity: %d%%", observation->humidity);
    mvwaddstr(w, 14, 2, str);

    snprintf(str, sizeof(str), "     Wind: %d %s", observation->wind_speed,
             observation->wind_direction_compass);
    mvwaddstr(w, 15, 2, str);

    snprintf(str, sizeof(str), "Visbility: %.2fkm", observation->visibility);
    mvwaddstr(w, 16, 2, str);

    snprintf(str, sizeof(str), "  UV risk: %s", observation->uv_description);
    mvwaddstr(w, 17, 2, str);

    box(w, '|', '-');
    attron(COLOR_PAIR(2) | A_BOLD);
    mvwaddstr(w, 0, 2, "Current Conditions");
    attroff(COLOR_PAIR(2) | A_BOLD);
    wrefresh(w);
  } else {
    box(w, '|', '-');
    attron(COLOR_PAIR(2) | A_BOLD);
    mvwaddstr(w, 0, 1, "Waiting...");
    attroff(COLOR_PAIR(2) | A_BOLD);
    wrefresh(w);
  }
}

void update_forecast(WINDOW *w) {
  wclear(w);
  wrefresh(w);
}

void update_forecast_day(WINDOW *w, struct forecast_s *forecast, int i) {
  int x;
  char str[100], d[20], sunrise[15], sunset[15], moonrise[15], moonset[15];

  x = getmaxx(w);

  wclear(w);

  mvwhline(w, 0, 0, '-', x);

  attron(COLOR_PAIR(2) | A_BOLD);
  strftime(d, 20, "%b %e, %A", &forecast->days[i].valid_date);
  snprintf(str, sizeof(str), " %s, %s ", d, forecast->days[i].moon_phrase);
  mvwaddstr(w, 0, 2, str);
  attroff(COLOR_PAIR(2) | A_BOLD);

  strftime(sunrise, 15, "%R", &forecast->days[i].sunrise);
  strftime(sunset, 15, "%R", &forecast->days[i].sunset);
  strftime(moonrise, 15, "%R", &forecast->days[i].moonrise);
  strftime(moonset, 15, "%R", &forecast->days[i].moonset);
  snprintf(str, sizeof(str), "Sun/moon: %s-%s, %s-%s", sunrise, sunset,
           moonrise, moonset);
  mvwaddstr(w, 1, 0, str);

  mvwaddstr(w, 2, 0, forecast->days[i].day.narrative);

  wrefresh(w);
}

void usage() {
  printf(
      "Usage: cweather [options]\n"
      "\n"
      "Shows a weather forecast.\n"
      "\n"
      "options:\n"
      "  -l <latitude,longitude> specify the location\n"
      "  -i <seconds> specify the interval for updates (default %d, minimum "
      "%d)\n",
      DEFAULT_INTERVAL, MINIMUM_INTERVAL);
}

int main(int argc, char **argv) {
  int i, c, rc;
  char location[50], *s, path[100];
  int interval;
  size_t path_len;
  struct stat st;
  dictionary *cfg;
  const char *cfg_s;
  int cfg_i;
  struct observation_s observation;
  struct forecast_s forecast;
  WINDOW *mw, *cw, *fw, *dw[14];
  fd_set rfds;
  struct timeval tv;
  time_t t;

  memset(location, 0, sizeof(location));
  memset(&observation, 0, sizeof(observation));
  memset(&forecast, 0, sizeof(forecast));

  strncpy(location, DEFAULT_LOCATION, sizeof(location));
  interval = DEFAULT_INTERVAL;

  memset(path, 0, sizeof(path));

  if ((s = getenv("HOME")) != NULL && (path_len = strlen(s)) > 0 &&
      path_len + 1 + strlen(CONFIG_FILE) < sizeof(path)) {
    strncat(path, s, sizeof(path));
    path[path_len] = '/';
    strncat(&(path[path_len + 1]), CONFIG_FILE, sizeof(path) - (path_len + 1));

    if ((rc = stat(path, &st)) == 0) {
      if ((cfg = iniparser_load(path)) != NULL) {
        if ((cfg_s = iniparser_getstring(cfg, "cweather:location", "")) !=
            NULL) {
          strncpy(location, cfg_s, sizeof(location));
        }

        if ((cfg_i = iniparser_getint(cfg, "cweather:interval", 0)) != 0) {
          interval = cfg_i;
        }

        iniparser_freedict(cfg);
      }
    }
  }

  if ((s = getenv("LOCATION")) != NULL && strlen(s) > 0) {
    strncpy(location, s, sizeof(location));
  }
  if ((s = getenv("INTERVAL")) != NULL && strlen(s) > 0) {
    interval = atoi(s);
  }

  while ((c = getopt(argc, argv, "l:i:")) != -1) {
    switch (c) {
      case 'l':
        strncpy(location, optarg, sizeof(location));
        break;
      case 'i':
        interval = atoi(optarg);
        break;
      case '?':
        usage();
        exit(0);
        break;
      default:
        usage();
        exit(1);
        break;
    }
  }

  if (strlen(location) == 0) {
    printf("Error: location not specified\n\n");
    usage();
    exit(1);
  }

  if (interval < MINIMUM_INTERVAL) {
    printf("Error: interval must be at least %d\n\n", MINIMUM_INTERVAL);
    usage();
    exit(1);
  }

  initscr();
  cbreak();
  noecho();
  curs_set(0);

  start_color();
  use_default_colors();
  init_pair(1, COLOR_WHITE, COLOR_BLUE);
  init_pair(2, COLOR_YELLOW, COLOR_RED);
  init_pair(3, COLOR_YELLOW, COLOR_BLACK);
  init_pair(4, COLOR_GREEN, COLOR_BLACK);

  mw = subwin(stdscr, LINES, COLS, 0, 0);
  cw = derwin(mw, LINES, 26, 0, 0);
  fw = derwin(mw, LINES, COLS - 26, 0, 26);
  for (i = 0; i < 14; i++) {
    dw[i] = derwin(fw, 4, COLS - 28, i * 4, 1);
  }

  keypad(stdscr, TRUE);
  nodelay(stdscr, TRUE);

  while (1) {
    if (t != 0) {
      FD_ZERO(&rfds);
      FD_SET(0, &rfds);

      tv.tv_sec = 0;
      tv.tv_usec = 500000;

      rc = select(1, &rfds, NULL, NULL, &tv);
      if (rc == -1) {
        perror("select()");
        break;
      }

      if (rc) {
        switch ((c = wgetch(stdscr))) {
          case 'q':
            endwin();
            return 0;
          case 'u':
            t = 0;
            break;
        }
      }
    }

    if ((time(NULL) - t) >= interval) {
      update_current(cw, &observation, interval, 0);

      if ((rc = fetch_observation(location, &observation)) != 0) {
        update_current(cw, &observation, interval, -1);
      }

      if ((rc = fetch_forecast(location, &forecast)) != 0) {
        update_current(cw, &observation, interval, -1);
      }

      t = time(NULL);

      update_current(cw, &observation, interval, t);
      update_forecast(fw);
      for (i = 0; i < 14; i++) {
        update_forecast_day(dw[i], &forecast, i);
      }
    } else {
      update_current(cw, &observation, interval, t);
    }

    refresh();
  }

  endwin();

  return 0;
}
