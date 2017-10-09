/*  =========================================================================
    nmea_count_aismsgtypes - Given an AIS NMEA text emits a CSV containing counts of the number of
messages it contained with each AIS message type

    Copyright (c) 2017 Inkblot Software Limited.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    nmea_count_aismsgtypes - Given an AIS NMEA text emits a CSV containing counts of the number of
messages it contained with each AIS message type
@discuss
@end
*/

#include "aisnmea_classes.h"


//  -- The range of AIS message types we cover (closed interval)

#define MIN_MSGTYPE 1
#define MAX_MSGTYPE 27

static bool
validtype (int msg_type) {
    return MIN_MSGTYPE <= msg_type && msg_type <= MAX_MSGTYPE;
}


//  --------------------------------------------------------------------------
//  Store for message_type counts

typedef struct MsgCounts {
    int counts [MAX_MSGTYPE + 1];
} MsgCounts;

static MsgCounts
MsgCounts_make () {
    return (MsgCounts) {0};
}

// Returns 0 on success, -1 on any failure (suggest you exit() on fail)
static int
MsgCounts_inc (MsgCounts *self, int msg_type) {
    if (! validtype (msg_type))
        return -1;

    self->counts [msg_type] += 1;
    return 0;
}

// Prints header row as well
static void
MsgCounts_print (MsgCounts *self) {
    printf ("\"message_type\",\"count\"\n");
    for (int i=0; i < MAX_MSGTYPE + 1; ++i) {
        if (self->counts [i])
            printf ("%d,%d\n", i, self->counts [i]);
    }
}
    

//  --------------------------------------------------------------------------
//  Log message and die

static void
bail (const char *msg, const char *nmea)
{
    assert (msg);
    if (nmea)
        fprintf (stderr, "ERROR: %s  nmea was: %s\n", msg, nmea);
    else
        fprintf (stderr, "ERROR: %s\n", msg);
    exit (1);
}


//  --------------------------------------------------------------------------
//  main()

// TODO consider loosening requirement that each input message is valid NMEA
// and has a valid AIS message type

int main (int argc, char *argv [])
{
    // Accept no arguments, exit with usage message if have some
    if (argc != 1) {
        puts ("USAGE:");
        puts ("  nmea_count_aismsgtypes < FILE.nmea");
        exit (1);
    }

    MsgCounts counts = MsgCounts_make ();

    aisnmea_t *parser = aisnmea_new (NULL);
    assert (parser);

    zfile_t *stdinf = zfile_new (NULL, "/dev/stdin");
    if (!stdinf)
        bail ("Problem opening stdin", NULL);
    int rc = zfile_input (stdinf);
    if (rc)
        bail ("Problem opening stdin", NULL);

    const char *line = zfile_readln (stdinf);
    if (!line)
        bail ("No data provided", NULL);

    // Actually process the lines
    while (line) {
        // We demand that every message in the file parses
        int rc = aisnmea_parse (parser, line);
        if (rc)
            bail ("NMEA parse failed", line);

        // We only care about first-fragnum messages
        if (aisnmea_fragnum (parser) != 1)
            goto cont;

        int mt = aisnmea_aismsgtype (parser);

        // We demand that each message has a valid AIS type
        if (! validtype (mt))
            bail ("Invalid ais message type", line);

        rc = MsgCounts_inc (&counts, mt);
        if (rc) {
            assert (0);  // this really shouldn't happen
            bail ("Error incrementing MsgCount", NULL);  // for NDEBUG
        }

    cont:
        line = zfile_readln (stdinf);
    }

    zfile_destroy (&stdinf);
    aisnmea_destroy (&parser);

    MsgCounts_print (&counts);

    return 0;
}
