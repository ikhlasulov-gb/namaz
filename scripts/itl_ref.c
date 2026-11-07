/* Minimal reference driver for the bundled ITL C implementation.
 *
 * Usage: itl_ref LAT LON TZ METHOD YEAR MONTH DAY
 * Prints six prayer times as HH:MM:SS, one per line.
 */
#include <stdio.h>
#include <stdlib.h>

#include "prayer.h"

int main(int argc, char **argv) {
    if (argc != 8) {
        fprintf(stderr, "usage: %s LAT LON TZ METHOD YEAR MONTH DAY\n", argv[0]);
        return 2;
    }
    Location location;
    Method method;
    Date date;
    Prayer prayers[6];

    date.day = atoi(argv[7]);
    date.month = atoi(argv[6]);
    date.year = atoi(argv[5]);
    location.degreeLat = atof(argv[1]);
    location.degreeLong = atof(argv[2]);
    location.gmtDiff = atof(argv[3]);
    location.dst = 0;
    location.seaLevel = 0;
    location.pressure = 1010;
    location.temperature = 10;

    getMethod(atoi(argv[4]), &method);
    getPrayerTimes(&location, &method, &date, prayers);

    for (int i = 0; i < 6; ++i) {
        printf("%02d:%02d:%02d\n", prayers[i].hour, prayers[i].minute,
               prayers[i].second);
    }
    return 0;
}
