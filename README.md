# aisnmea
Simple, portable and fast parser for AIS-holding NMEA data, written in pure C

Handles the outer NMEA layer, exposing the internal data for subsequent
processing.

In most cases you probably want to grab metadata from the
tag block about how the messages were received, and the the body
and fillbits fields from the core message to pass to a wire format
decoder.

To understand the format better see [here](http://catb.org/gpsd/AIVDM.html).

If you want to parse the wire format, try [libais](https://github.com/schwehr/libais/).
(If you need better permformance you can use its C++ classes directly,
rather than going via its Python interface.)

This project is currently built and tested on Linux, but it really shouldn't
be difficult to make it work on Windows, since it's based on
[zproject](https://github.com/zeromq/zproject/) and [CZMQ](https://github.com/zeromq/czmq/).

We also ship the utility program `nmea_count_aismsgtypes`, described below, which
counts the number of messages of each AIS message type existing in a provided
AIS NMEA text.


Example
-------

```c
aisnmea_t *msg = aisnmea_new ("\\g:1-2-73874,n:157036,s:r003669945,c:1241544035*4A"
                              "\\!AIVDM,1,1,,B,15N4cJ`005Jrek0H@9n`DW5608EP,0*13");
assert (msg);  // Constuctor returns NULL on parse failure

// Each core field has a named accessor

assert (streq ("!AIVDM", aisnmea_head (msg)));
assert (   1 == aisnmea_fragcount (msg));
assert (   1 == aisnmea_fragnum (msg));
assert (  -1 == aisnmea_messageid (msg));
assert ( 'B' == aisnmea_channel (msg));
assert (streq ("15N4cJ`005Jrek0H@9n`DW5608EP", aisnmea_payload (msg)));
assert (   0 == aisnmea_fillbits (msg));
assert (0x13 == aisnmea_checksum (msg));
assert (   1 == aisnmea_aismsgtype (msg));

// And tag block values can be accessed by key

assert (streq (aisnmea_tagblockval (msg, "g"),
               "1-2-73874"));
assert (NULL = aisnmea_tagblockval (msg, "nono"));  // NULL if key not present

// We can handle messages both with and without tag blocks.
// Note that we resuse the original parser here, which can give cleaner code
// if you have a lot of lines to process.

int err = aisnmea_parse (msg, "!AIVDM,2,1,3,B,55P5TL01VIaAL@7WKO@mBpl"
                              "U@<PDhh000000001S;AJ::4A80?4i@E53,0*3E");
assert (!err);

// Make sure to call the message destructor at the end
aisnmea_destroy (&msg);
```


nmea_count_aismsgtypes
----------------------

```shell
USAGE:
  nmea_count_aismsgtypes < FILE.nmea
```

Counts the number of messages in the NMEA text provided on stdin with each AIS
message type, and outputs the counts as CSV.

The CSV only contains rows for which the relevant count was >0, and these
rows contain the following columns: {msg_type, count}. A header row containing
{"message_type","count"} is also included.

Counts only the first part of multipart AIS messages, so if you're missing
later parts the resulting counts will arguably be artifically high, or vice
versa if you're missing the first part.


Installation
------------

The easiest way is to git clone this repository, then use the included
Autotools or CMake scripts.

Using the former:

```bash
./autogen.sh
./configure
make
sudo make install
```

Now you should have aisnmea.h and libaisnmea.so/.a in /usr/local.

You can also use `make check` to run the project self tests.


Rationale
---------

Exploratory data science with AIS data often involves building a lot of specal-
purpose command line tools, and sometimes then later turning these into a more
stable, scalable data pipeline.

In order to do this efficiently we need libraries which are:

- Trivially easy to use
- Fast, so we can process the data over and over again to simplify architecture
- Accessible from any language via FFI without too much work

The easiest way to meet all of these objectives is with a portable C library;
just install and add a single linker flag to your build.

We follow the [CLASS](https://rfc.zeromq.org/spec:21/CLASS/) style for this
project.


Dependencies
------------

First install [CZMQ](https://github.com/zeromq/czmq/), which is very likely in
your distro's package repositories.

Otherwise building from source is fairly easy; see the instructions there.


Complete API
------------

```c
//  Parse an NMEA string and return the results, or NULL if the parse failed.
//  Pass NULL for string argument to construct in default state.
AISNMEA_EXPORT aisnmea_t *
    aisnmea_new (const char *nmea);

//  Destroy the aisnmea.
AISNMEA_EXPORT void
    aisnmea_destroy (aisnmea_t **self_p);

//  Parse an NMEA string, reusing the current parser, replacing its contents
//  with the new parsed data.
//
//  Returns 0 on success, or -1 on parse failure. Object state after a failed
//  parse is undefined.
AISNMEA_EXPORT int
    aisnmea_parse (aisnmea_t *self, const char *nmea);

// Accessors:

//  Get the string in the tagblock with given key.
//  Returns NULL if key not found or if there was no tagblockl.
AISNMEA_EXPORT const char *
    aisnmea_tagblockval (aisnmea_t *self, const char *key);

//  Sentence identifier, e.g. "!AIVDM"
//  TODO consider stripping the leading '!'; depends on what clients want.
AISNMEA_EXPORT const char *
    aisnmea_head (aisnmea_t *self);

//  How many fragments in the message sentence containing this one?
AISNMEA_EXPORT size_t
    aisnmea_fragcount (aisnmea_t *self);

//  Which fragment number of the whole sentence is this one? (One-based)
AISNMEA_EXPORT size_t
    aisnmea_fragnum (aisnmea_t *self);

//  Sequential message ID, for multi-sentence messages.
//  Often (intentionally) missing, in which case we return -1.
AISNMEA_EXPORT int
    aisnmea_messageid (aisnmea_t *self);

//  Radio channel message was transmitted on (NB not the same as
//  unit class, this is about frequency).
//  Theoretically only 'A' and 'B' are allowed, but '1' and '2'
//  are seen, which mean the same things.
AISNMEA_EXPORT char
    aisnmea_channel (aisnmea_t *self);

//  Data payload for the message. This is where the AIS meat lies.
//  Pass this to an AIS message decoding library.
AISNMEA_EXPORT const char *
    aisnmea_payload (aisnmea_t *self);

//  Number of padding bits included at the end of the payload.
//  The AIS decoding library needs to know this number, so it can strip
//  them off.
AISNMEA_EXPORT size_t
    aisnmea_fillbits (aisnmea_t *self);

//  Message checksum. Transmitted in hex.
AISNMEA_EXPORT size_t
    aisnmea_checksum (aisnmea_t *self);

//  Returns the AIS message type of the message, or -1 if the message
//  doesn't exhibit a valid AIS messgae type.
//  (This is worked out from the first character of the payload.)
AISNMEA_EXPORT int
    aisnmea_aismsgtype (aisnmea_t *self);

// Class self test:

//  Self test of this class.
AISNMEA_EXPORT void
    aisnmea_test (bool verbose);
```



