//
// Created by Camleoah on 29/10/2022.
//

#include "TinyGPSPlus.h"
#include "HardwareSerial.h"
#include "gps_manager.h"
#include "tools/loop_timer.h"
#include "wifi_handler.h"
#include "ram_log.h"

TinyGPSPlus gps_obj;
HardwareSerial SerialGPS(1);

uint32_t counter_gps_update = 0;

GpsDataState_t gpsState = {};

// This custom version of delay() ensures that the gps object
// is being "fed".
static void _smart_delay(unsigned long ms) {
    unsigned long start = millis();
    do {
        while (SerialGPS.available())
            gps_obj.encode(SerialGPS.read());
    } while (millis() - start < ms);
}

GPS_MANAGER_ERROR_t _update_pos(unsigned long timeout, uint8_t sats) {
    unsigned long start = millis();
    do {
        // pull data
        while (SerialGPS.available())
            gps_obj.encode(SerialGPS.read());

        // check for satellites
        if (gps_obj.satellites.isValid() && sats <= gps_obj.satellites.value()) {
            // notify ramlog
            if (sats > gpsState.numberSats) {
                String str_log = "Satelliten-Genauigkeit hergestellt. Satelliten: " + String(gps_obj.satellites.value());
            }

            // update number of satellites
            gpsState.numberSats = gps_obj.satellites.value();

            // check for validity
            if (gps_obj.location.isValid() && gps_obj.location.isUpdated()) {
                // measurement is valid and fresh, therefore save position
                gpsState.posLat = gps_obj.location.lat();
                gpsState.posLon = gps_obj.location.lng();
                // all done, so return out of loop
                return GPS_MANAGER_ERROR_NO_ERROR;
            }
        }
    } while (millis() - start < timeout);
    // we timeouted. the position measurement is not valid
    if (gpsState.numberSats < sats)
        return GPS_MANAGER_ERROR_SATS;
    return GPS_MANAGER_ERROR_LOCATION;
}


void gps_manager_init() {
    // init serial communication with gps module
    SerialGPS.begin(GPS_SERIAL_BAUD_RATE, SERIAL_8N1, PIN_RX, PIN_TX);

#ifdef SYS_CONTROL_SAVE_MILAGE
    // read milage from flash
    long readValue;
    EEPROM_readAnything(12, readValue);
    gpsState.milage_km = (double)readValue / 1000000;
#endif

    // wait for gps module
    DualSerial.println("Warte auf erste GPS daten...");
    while (!SerialGPS.available())
        delay(100);

    DualSerial.println("GPS online.");
}

void gps_manager_update() {

    // we always pull data from the gps module
    _smart_delay(0);

    // but we only check the current position periodically
    counter_gps_update++;
    if (counter_gps_update * 1000 / FREQ_LOOP_CYCLE_HZ > INVERVAL_GPS_MEASURE_MS) {
        counter_gps_update = 0;

        GPS_MANAGER_ERROR_t retval;
        // read gps coordinates from module
        if ((retval = _update_pos(5000, 4)) == GPS_MANAGER_ERROR_NO_ERROR) {
            // we have a valid measurement so lets check whether we have a previous measurement
            if (gpsState.prevPosLat == 0 || gpsState.prevPosLon == 0) {
                // we don't have prev measurements yet so lets save them
                gpsState.prevPosLat = gpsState.posLat;
                gpsState.prevPosLon = gpsState.posLon;
                // and return early
                return;
            }

            // calculate route kilometers
            double interval_m = TinyGPSPlus::distanceBetween(gpsState.posLat, gpsState.posLon, gpsState.prevPosLat,
                                                             gpsState.prevPosLon);

            if (interval_m > INTERVAL_DISTANCE_M) {
                // we have passed a distance of x meters
                String log = "Distanz zwischen " + String(gpsState.posLat) + ", " + String(gpsState.posLon) + " und " +
                             String(gpsState.prevPosLat) + ", " + String(gpsState.prevPosLon) + ": " +
                             String(interval_m) + ", Km: " + String(gpsState.milage_km);
                ram_log_notify(RAM_LOG_INFO, log.c_str());
                DualSerial.println(log.c_str());

                // update previous position
                gpsState.prevPosLat = gpsState.posLat;
                gpsState.prevPosLon = gpsState.posLon;

                // save to milage
                gpsState.milage_km += interval_m / 1000.0;

#ifdef SYS_CONTROL_SAVE_MILAGE
                long writeValue = gpsState.milage_km * 1000000;
                EEPROM_writeAnything(12, writeValue);
                EEPROM.commit(); // commit data to flash
#endif
            }
        }

        else if (retval == GPS_MANAGER_ERROR_SATS) {
            String str_log = "Satelliten-Genauigkeit verloren. Satelliten: " + String(gpsState.numberSats);
            ram_log_notify(RAM_LOG_INFO, str_log.c_str());
        }
    }
}