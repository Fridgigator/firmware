#include <esp_netif.h>
#include <lwip/apps/sntp.h>
#include <esp_task_wdt.h>
#include "ArduinoSupport.h"

void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

static void setTimeZone();

// based on https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-time.c
void configTime(const char *server1) {
    esp_netif_init();
    if (sntp_enabled()) {
        sntp_stop();
    }
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char *) server1);
    sntp_init();
    setTimeZone();
}

static void setTimeZone() {
    long offset = 0;
    long daylight = 0;
    char cst[17] = {0};
    char cdt[17] = "DST";
    char tz[33] = {0};

    if (offset % 3600) {
        sprintf(cst, "UTC%ld:%02ld:%02ld", offset / 3600, abs((offset % 3600) / 60), abs(offset % 60));
    } else {
        sprintf(cst, "UTC%ld", offset / 3600);
    }
    long tz_dst = offset - daylight;
    if (tz_dst % 3600) {
        sprintf(cdt, "DST%ld:%02ld:%02ld", tz_dst / 3600, abs((tz_dst % 3600) / 60), abs(tz_dst % 60));
    } else {
        sprintf(cdt, "DST%ld", tz_dst / 3600);
    }
    printf(tz, "\ntz:%s%s\n", cst, cdt);
    sprintf(tz, "%s%s", cst, cdt);
    setenv("TZ", tz, 1);
    tzset();
}

void yield() {

}