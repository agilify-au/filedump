/* ------------------------------------------------------------------------
 * dump.h - reusable hexadecimal-dump renderer.
 *
 * Link dump.c into any application that wants to render a hex dump of an
 * in-memory buffer.  Typical use:
 *
 *      #include "dump.h"
 *      dumpopts_t o;
 *      dumpinit(&o);                 // sensible defaults for this platform
 *      o.showEbcdic = true;          // tweak whatever you like
 *      dump(buf, len, 0, stdout, &o);
 *
 * The displayable-character columns are emitted as native character
 * literals, so on an ASCII host they come out ASCII and on z/OS (an EBCDIC
 * host) they come out EBCDIC - the same source is correct on both.  The
 * ASCII column interprets each byte as ASCII; the EBCDIC column interprets
 * each byte as IBM-1047.
 * --------------------------------------------------------------------- */
#ifndef DUMP_H
#define DUMP_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int  bytesPerLine;   /* bytes shown per output line                   */
    int  groupSize;      /* bytes per hex group (packed, no inner spaces) */
    int  groupGap;       /* spaces inserted between groups                */
    int  indent;         /* leading spaces on every line                  */
    int  offsetWidth;    /* field width of the offset column              */
    bool offsetHex;      /* true = hex offsets, false = decimal           */
    bool showAscii;      /* show the ASCII character column               */
    bool showEbcdic;     /* show the IBM-1047 character column            */
} dumpopts_t;

/* Populate opts with defaults: 16 bytes/line, groups of 4, one space
 * between groups, no indent, hex offsets.  The default character column is
 * EBCDIC (IBM-1047) on z/OS and ASCII everywhere else. */
void dumpinit(dumpopts_t *opts);

/* Render a hex dump of "length" bytes from "buffer".  startOffset is the
 * file/stream offset of buffer[0], so callers that read in chunks pass the
 * running offset and the offset column stays correct.  Output goes to out.
 * Runs of identical lines collapse to a single '*' (xxd/hexdump style). */
void dump(const uint8_t *buffer, uint64_t length, uint64_t startOffset,
          FILE *out, const dumpopts_t *opts);

/* Number of digits needed to print maxOffset in the chosen radix (>= 1).
 * Handy for sizing offsetWidth from a known total length. */
int dumpwidth(uint64_t maxOffset, bool hex);

#ifdef __cplusplus
}
#endif

#endif /* DUMP_H */
