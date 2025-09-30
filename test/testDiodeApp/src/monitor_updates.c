/*
 * Subscribe to N records ("<name>1" to "<name><N>")
 * using EPICS Channel Access (CA) and DBR_TIME_DOUBLE.
 * Detects missed values (skips in increment).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <epicsStdlib.h>
#include <cadef.h>
#include <db_access.h>

char timeText[32];
const char* get_timestamp()
{
    epicsTimeStamp tsNow;

    epicsTimeGetCurrent(&tsNow);
    epicsTimeToStrftime(timeText, sizeof(timeText)/sizeof(char), "%Y-%m-%dT%H:%M:%S.%03f", &tsNow);

    return timeText;
}    

#define MAX_CHANNELS 50000

static chid channels[MAX_CHANNELS];
static evid subscriptions[MAX_CHANNELS];

/* Track last seen value per record */
static double last_val[MAX_CHANNELS];
static int initialized[MAX_CHANNELS] = {0};

static int channels_connected = 0;
static int missed = 0;
static int updates = 0;

static void connection_callback(struct connection_handler_args args) {
    if (args.op == CA_OP_CONN_UP) {
        channels_connected++;
    } else if (args.op == CA_OP_CONN_DOWN) {
        channels_connected--;
    }
}

static void event_callback(struct event_handler_args args) {
    if (args.status != ECA_NORMAL) {
        fprintf(stderr, "CA event error for %s: %s\n",
                ca_name(args.chid), ca_message(args.status));
        return;
    }

    struct dbr_time_double *val = (struct dbr_time_double *) args.dbr;
    int idx = (int)(intptr_t) args.usr;  /* record index 0..n_channels-1 */

    if (!initialized[idx]) {
        last_val[idx] = val->value;
        initialized[idx] = 1;
        return;
    }

    double expected = last_val[idx] + 1.0;
    if (val->value != expected) {
        missed += (int)(val->value - expected);
        //printf("MISSING update on %s: got %.0f, expected %.0f (delta=%.0f)\n",
        //       ca_name(args.chid), val->value, expected, val->value - last_val[idx]);
    }

    last_val[idx] = val->value;
    updates++;
}

int main(int argc, char **argv) {
    int status;
    char *prefix;
    int n_channels;
    double settle_time = 0.0;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <prefix> <n_channels> [settle_time]\n", argv[0]);
        fprintf(stderr, "  prefix: PV name prefix (e.g., 'xrec')\n");
        fprintf(stderr, "  n_channels: number of channels to monitor (max %d)\n", MAX_CHANNELS);
        fprintf(stderr, "  settle_time: optional settle time in seconds before reporting missing updates\n");
        return 1;
    }

    prefix = argv[1];
    n_channels = atoi(argv[2]);

    if (argc == 4) {
        settle_time = atof(argv[3]);
        if (settle_time < 0) {
            fprintf(stderr, "Error: settle_time must be non-negative\n");
            return 1;
        }
    }

    if (n_channels <= 0 || n_channels > MAX_CHANNELS) {
        fprintf(stderr, "Error: n_channels must be between 1 and %d\n", MAX_CHANNELS);
        return 1;
    }

    SEVCHK(ca_context_create(ca_enable_preemptive_callback), "ca_context_create");

    printf("%s Subscribing to %s[1-%d] PVs...\n", get_timestamp(), prefix, n_channels);

    char pv[128];
    for (int i = 1; i <= n_channels; i++) {
        snprintf(pv, sizeof(pv), "%s%d", prefix, i);

        status = ca_create_channel(pv, connection_callback, NULL, CA_PRIORITY_DEFAULT, &channels[i-1]);
        if (status != ECA_NORMAL) {
            fprintf(stderr, "Could not create channel %s: %s\n", pv, ca_message(status));
        }
    }

    for (int i = 0; channels_connected < n_channels && i < 15; i++) {
        ca_pend_event(1.0);
        printf("%s Connected to %d PVs.\n", get_timestamp(), channels_connected);
    }

    if (channels_connected < n_channels) {
        printf("%s Warning: Only connected to %d of %d channels.\n",
               get_timestamp(), channels_connected, n_channels);
    }

    for (int i = 0; i < n_channels; i++) {
        status = ca_create_subscription(DBR_TIME_DOUBLE, 1, channels[i],
                                        DBE_VALUE,
                                        event_callback, (void*)(intptr_t)i,
                                        &subscriptions[i]);
        if (status != ECA_NORMAL) {
            fprintf(stderr, "Subscription failed for %s: %s\n",
                    ca_name(channels[i]), ca_message(status));
        }
    }

    printf("%s Subscribed to %d PVs.\n", get_timestamp(), n_channels);
    if (settle_time > 0) {
        printf("%s Settle time: %.1f second(s)\n", get_timestamp(), settle_time);
    } else {
        printf("%s Starting to report missing updates.\n", get_timestamp());
    }

    double pend_event_timeout = 0.025;  // 25 ms
    int iterations_per_sec = (int)(1 / pend_event_timeout);
    int settle_iterations = (int)(settle_time / pend_event_timeout);
    int iteration = 0;
    while (1) {
        ca_pend_event(pend_event_timeout);

        if (settle_iterations > 0) {
            settle_iterations--;
            if (settle_iterations <= 0) {
                printf("%s Settle period complete. Starting to report missing updates.\n", get_timestamp());
                missed = 0;
            }
        } else if (++iteration == iterations_per_sec) {
            if (updates == 0) {
                printf("%s No updates received in the last second (connected: %d/%d).\n",
                       get_timestamp(), channels_connected, n_channels);
            } else if (missed) {
                printf("%s Missed updates/sec: %d\n",
                       get_timestamp(), missed);
            }
            missed = 0;
            updates = 0;
            iteration = 0;
        }
    }

    ca_context_destroy();
    return 0;
}

