/* ------------------------------------------------------------------------
 * filedump - command-line hexadecimal dump utility.
 *
 * Opens the input file and feeds its bytes to dump() (see dump.c/dump.h),
 * whole, in chunks, by MVS record (z/OS), or by LLZZ variable record.
 *
 * Build: see Makefile.linux / Makefile.win / Makefile.zos.
 * --------------------------------------------------------------------- */
#include "dump.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
#endif

#define BUFSZ (64 * 1024)   /* read chunk; also the max LLZZ record size */

static void usage(FILE *fp, const char *prog)
{
    fprintf(fp,
        "Usage: %s [options] file        (use \"-\" for standard input)\n"
        "\n"
        "Display options:\n"
        "  -w N   bytes per line               (default 16)\n"
        "  -g N   bytes per hex group          (default 4)\n"
        "  -s N   spaces between groups        (default 1)\n"
        "  -i N   indent each line by N spaces (default 0)\n"
        "  -d     decimal offsets              (default hex)\n"
        "  -x     hex offsets                  (default)\n"
        "  -a     show ASCII character column\n"
        "  -e     show EBCDIC (IBM-1047) column\n"
        "  -n     no character column (hex only)\n"
#if defined(__MVS__)
        "         (column defaults to EBCDIC on z/OS or with -v, else ASCII)\n"
#else
        "         (column defaults to ASCII, or to EBCDIC when -v is used)\n"
#endif
        "\n"
        "Input options:\n"
        "  -v     variable-length records: each record is LLZZ + data,\n"
        "         LL = 2-byte big-endian total length (incl. the 4-byte\n"
        "         LLZZ); ZZ is shown but not interpreted (any platform)\n"
        "  -l     with -v, include the 4-byte LLZZ in the dumped data\n"
        "         (default dumps data only; LL/ZZ always appear in the\n"
        "          record header line either way)\n"
#if defined(__MVS__)
        "  -r     read MVS data set record by record (z/OS)\n"
#endif
        "  -h     this help\n",
        prog);
}

static int parse_uint(const char *s, const char *prog, char opt)
{
    char *end;
    long v;
    if (s == NULL || *s == '\0') {
        fprintf(stderr, "%s: option -%c requires a value\n", prog, opt);
        exit(12);
    }
    v = strtol(s, &end, 10);
    if (*end != '\0' || v < 0) {
        fprintf(stderr, "%s: invalid value '%s' for -%c\n", prog, s, opt);
        exit(12);
    }
    return (int)v;
}

/* best-effort file size (cosmetic: sizes the offset column) */
static long long file_size(FILE *f)
{
#if defined(_WIN32)
    long long s;
    if (_fseeki64(f, 0, SEEK_END) != 0) return -1;
    s = _ftelli64(f);
    if (_fseeki64(f, 0, SEEK_SET) != 0) return -1;
    return s;
#else
    long s;
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    s = ftell(f);
    if (s < 0 || fseek(f, 0, SEEK_SET) != 0) return -1;
    return (long long)s;
#endif
}

int main(int argc, char **argv)
{
    dumpopts_t  opts;
    const char *prog = (argc > 0) ? argv[0] : "filedump";
    const char *fn   = NULL;
    bool record_mode = false;     /* -r : z/OS record mode      */
    bool var_mode    = false;     /* -v : LLZZ variable records */
    bool no_chars    = false;     /* -n                          */
    bool sawAscii    = false;     /* user gave -a                */
    bool sawEbcdic   = false;     /* user gave -e                */
    bool include_llzz = false;    /* -l : dump LLZZ too (with -v) */
    uint8_t *buf;
    FILE *f;
    int i;

    /* 1. no parameters at all -> show usage */
    if (argc < 2) {
        usage(stderr, prog);
        return 8;
    }

    dumpinit(&opts);              /* defaults (EBCDIC column on z/OS) */

    /* ---- option parsing (supports -w16 and -w 16) --------------------- */
    i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        const char *a = argv[i] + 1;
        if (a[0] == '-' && a[1] == '\0') { i++; break; }   /* "--" */
        while (*a) {
            char opt = *a++;
            switch (opt) {
            case 'a': sawAscii  = true; break;
            case 'e': sawEbcdic = true; break;
            case 'n': no_chars  = true; break;
            case 'd': opts.offsetHex = false; break;
            case 'x': opts.offsetHex = true;  break;
            case 'v': var_mode    = true; break;
            case 'l': include_llzz = true; break;
            case 'r': record_mode = true; break;
            case 'h': case '?': usage(stdout, prog); return 0;
            case 'w': case 'g': case 's': case 'i': {
                const char *val = a;
                if (*val == '\0') { val = (i + 1 < argc) ? argv[++i] : ""; }
                {
                    int num = parse_uint(val, prog, opt);
                    if      (opt == 'w') opts.bytesPerLine = num;
                    else if (opt == 'g') opts.groupSize    = num;
                    else if (opt == 's') opts.groupGap     = num;
                    else                 opts.indent       = num;
                }
                a = "";   /* value consumed the rest of this token */
                break;
            }
            default:
                fprintf(stderr, "%s: unknown option -%c\n", prog, opt);
                usage(stderr, prog);
                return 12;
            }
        }
        i++;
    }

    if (i >= argc) { fprintf(stderr, "%s: no file name provided\n", prog); return 12; }
    fn = argv[i];

    /* ---- resolve character-column selection --------------------------- */
    if (no_chars) {
        opts.showAscii = false;
        opts.showEbcdic = false;
    } else if (sawAscii || sawEbcdic) {     /* explicit -a/-e overrides default */
        opts.showAscii  = sawAscii;
        opts.showEbcdic = sawEbcdic;
    } else if (var_mode) {                   /* RDW/LLZZ data is EBCDIC */
        opts.showAscii  = false;
        opts.showEbcdic = true;
    }                                        /* else keep dumpinit() default */

    /* ---- validate ----------------------------------------------------- */
    if (record_mode && var_mode) {
        fprintf(stderr, "%s: -r and -v cannot both be specified\n", prog);
        return 12;
    }
#if !defined(__MVS__)
    if (record_mode) {
        fprintf(stderr, "%s: -r (record mode) is only supported on z/OS\n", prog);
        return 12;
    }
#endif
    if (opts.bytesPerLine < 1 || opts.bytesPerLine > 4096) {
        fprintf(stderr, "%s: -w must be 1..4096\n", prog); return 12;
    }
    if (opts.groupSize < 1) opts.groupSize = opts.bytesPerLine;

    /* ---- open --------------------------------------------------------- */
    if (strcmp(fn, "-") == 0) {
        f = stdin;
#if defined(_WIN32)
        _setmode(_fileno(stdin), _O_BINARY);
#endif
    } else if (record_mode) {
        f = fopen(fn, "rb, type=record");
    } else {
        f = fopen(fn, "rb");
    }
    if (f == NULL) { perror("ERROR: cannot open input file"); return 12; }

    buf = (uint8_t *)malloc(BUFSZ);
    if (buf == NULL) { fprintf(stderr, "%s: out of memory\n", prog); fclose(f); return 12; }

    /* ---- drive dump() per mode ---------------------------------------- */
    if (var_mode) {
        /* LLZZ variable records.  LL = big-endian total length INCLUDING
         * the 4-byte LLZZ (classic RDW).  By default only the data payload
         * (LL-4 bytes) is dumped; -l includes the 4-byte LLZZ in the dump.
         * Either way LL/ZZ are echoed in the record header.  No logic keys
         * off ZZ. */
        long recno = 0;
        for (;;) {
            size_t got = fread(buf, 1, 4, f);
            unsigned ll, zz, datalen;
            const uint8_t *dptr;
            uint64_t       dlen;
            if (got == 0) break;                         /* clean EOF */
            if (got < 4) {
                fprintf(stderr, "ERROR: truncated LLZZ header (%lu byte(s))\n",
                        (unsigned long)got);
                fclose(f); free(buf); return 12;
            }
            ll = ((unsigned)buf[0] << 8) | buf[1];       /* big-endian */
            zz = ((unsigned)buf[2] << 8) | buf[3];
            if (ll < 4) {
                fprintf(stderr, "ERROR: bad RDW length %u (< 4)\n", ll);
                fclose(f); free(buf); return 12;
            }
            if (ll > BUFSZ) {
                fprintf(stderr, "ERROR: record length %u exceeds buffer %d\n", ll, BUFSZ);
                fclose(f); free(buf); return 12;
            }
            datalen = ll - 4;
            got = fread(buf + 4, 1, datalen, f);
            if (got != datalen) {
                fprintf(stderr, "ERROR: truncated record (wanted %u, got %lu)\n",
                        datalen, (unsigned long)got);
                fclose(f); free(buf); return 12;
            }
            recno++;
            dptr = include_llzz ? buf       : buf + 4;
            dlen = include_llzz ? ll        : datalen;
            opts.offsetWidth = dumpwidth(dlen ? dlen - 1 : 0, opts.offsetHex);
            if (opts.offsetWidth < 4) opts.offsetWidth = 4;
            fprintf(stdout, "\nRecord %ld  (LL=%u [0x%04X], ZZ=0x%04X, data=%u)\n",
                    recno, ll, ll, zz, datalen);
            dump(dptr, dlen, 0, stdout, &opts);
        }
    }
#if defined(__MVS__)
    else if (record_mode) {
        long recno = 0;
        for (;;) {
            size_t got = fread(buf, 1, BUFSZ, f);        /* one record per read */
            if (got == 0) break;
            recno++;
            opts.offsetWidth = dumpwidth(got ? got - 1 : 0, opts.offsetHex);
            if (opts.offsetWidth < 4) opts.offsetWidth = 4;
            fprintf(stdout, "\nRecord %ld  (length %lu bytes)\n",
                    recno, (unsigned long)got);
            dump(buf, (uint64_t)got, 0, stdout, &opts);   /* offset restarts at 0 */
        }
    }
#endif
    else {
        /* plain: read in chunks, keep a running file offset */
        long long sz = file_size(f);
        uint64_t off = 0;
        int w = (sz > 0) ? dumpwidth((uint64_t)sz - 1, opts.offsetHex)
                         : (opts.offsetHex ? 8 : 10);
        opts.offsetWidth = (w < 6) ? 6 : w;
        for (;;) {
            size_t got = fread(buf, 1, BUFSZ, f);
            if (got == 0) break;
            dump(buf, (uint64_t)got, off, stdout, &opts);
            off += got;
        }
    }

    if (ferror(f)) perror("ERROR: read error");
    if (f != stdin) fclose(f);
    free(buf);
    return 0;
}
