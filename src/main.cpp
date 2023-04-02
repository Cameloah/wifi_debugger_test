#include <Arduino.h>
#include <cstdio>
#include "ESPAsyncWebServer.h"

#include "wifi_handler.h"
#include "version.h"
#include "github_update.h"
#include "tools/loop_timer.h"
#include "user_interface.h"
#include "ram_log.h"


// debug and system control options
#define SYSCTRL_LOOPTIMER               // enable loop frequency control, remember to also set the loop freq in the loop_timer.h
#define INTERVAL_WIFI_CHECK_MS          60000

void handleRoot(AsyncWebServerRequest *request)
{request->send(200, "text/html","blabliblubb");
}


void setup() {
    delay(1000);
    // Setup DualSerial communication
    DualSerial.begin(115200);

    // wifi setup
    uint8_t retval = wifi_handler_init();

    if(retval == WIFI_HANDLER_ERROR_NO_ERROR) {
        DualSerial.println("Suche nach Updates...");
        retval = github_update_checkforlatest();
        if (retval == GITHUB_UPDATE_ERROR_NO_ERROR)
            github_update_firmwareUpdate();
        else if (retval == GITHUB_UPDATE_ERROR_NO_UPDATE)
            DualSerial.println("FW ist aktuell!");
        else {
            DualSerial.println("Fehler.");
            ram_log_notify(RAM_LOG_ERROR_GITHUB_UPDATE, retval);
        }

        // since we have Wi-Fi, lets start the server
        ram_log_notify(RAM_LOG_INFO, "Starte Server", true);
        server.on("/", HTTP_GET, handleRoot);
        server.begin();
    }
    else if (retval == WIFI_HANDLER_ERROR_CONNECT) {
        DualSerial.println("WLAN nicht gefunden.");
        ram_log_notify(RAM_LOG_ERROR_WIFI_HANDLER, retval);
    }
    else {
        ram_log_notify(RAM_LOG_ERROR_WIFI_HANDLER, retval);
        DualSerial.println("Fehler."); }

    DualSerial.println("Einsatzbereit!");
    DualSerial.println(URL_FW_VERSION);
    DualSerial.println(URL_FW_BIN);
    DualSerial.println(URL_FS_BIN);
}


double counter_wifi = 0;

void loop() {
    // save t_0 time stamp in loop_timer
    t_0 = micros();

    counter_wifi++;
    if (!wifi_handler_is_connected() && (counter_wifi * 1000 / FREQ_LOOP_CYCLE_HZ > INTERVAL_WIFI_CHECK_MS)) {
        counter_wifi = 0;

        // try to reconnect
        uint8_t retval = wifi_handler_connect();
        if (retval == WIFI_HANDLER_ERROR_NO_ERROR) {
            // setup root callback to send data
            ram_log_notify(RAM_LOG_INFO, "WiFi wieder verbunden. Starte Server.", true);
            server.on("/", handleRoot);
            server.begin();
        }
        else if (retval == WIFI_HANDLER_ERROR_CONNECT) {
            DualSerial.println("WLAN nicht gefunden.");
            delay(10000);

        }
        else {
            DualSerial.println("Fehler bei WLAN-Suche.");
            ram_log_notify(RAM_LOG_ERROR_WIFI_HANDLER, retval);
        }
    }

    // run wifi server routine
    wifi_handler_update();

    // listen for user input
    ui_serial_comm_handler();

    loop_timer++;   // iterate loop timer to track loop frequency

#ifdef SYSCTRL_LOOPTIMER
    // keep loop at constant cycle frequency
    loop_timer_check_cycle_freq();
#endif

}