/* ------------------------------------------------------------------------
 * dump.c - reusable hexadecimal-dump renderer.  See dump.h.
 * --------------------------------------------------------------------- */
#include "dump.h"

#include <stdlib.h>
#include <string.h>

#define NDC '.'     /* glyph shown for a non-displayable byte */

/* hex digits (native charset via literals) */
static const char hexdig[] = {
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};

/* ASCII: input byte interpreted as ASCII -> printable glyph */
static const unsigned char asctab[256] = {
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
' ','!','"','#','$','%','&','\'','(',')','*','+',',','-','.','/',
'0','1','2','3','4','5','6','7', '8','9',':',';','<','=','>','?',
'@','A','B','C','D','E','F','G', 'H','I','J','K','L','M','N','O',
'P','Q','R','S','T','U','V','W', 'X','Y','Z','[','\\',']','^','_',
'`','a','b','c','d','e','f','g', 'h','i','j','k','l','m','n','o',
'p','q','r','s','t','u','v','w', 'x','y','z','{','|','}','~',NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC
};

/* EBCDIC: input byte interpreted as IBM-1047 -> printable glyph.
 * Only code points mapping to printable ASCII (0x20-0x7E) get a glyph;
 * Latin-1 accents, cent, broken-bar, not-sign, etc. show as NDC.  Note the
 * IBM-1047 positions: 0x4F '|', 0x5A '!', brackets at 0xAD '[' / 0xBD ']'. */
static const unsigned char ebctab[256] = {
/*0x00*/ NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
/*0x10*/ NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
/*0x20*/ NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
/*0x30*/ NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC,
/*0x40*/ ' ',NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,'.','<','(','+','|',
/*0x50*/ '&',NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,'!','$','*',')',';','^',
/*0x60*/ '-','/',NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,',','%','_','>','?',
/*0x70*/ NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,'`',':','#','@','\'','=','"',
/*0x80*/ NDC,'a','b','c','d','e','f','g', 'h','i',NDC,NDC,NDC,NDC,NDC,NDC,
/*0x90*/ NDC,'j','k','l','m','n','o','p', 'q','r',NDC,NDC,NDC,NDC,NDC,NDC,
/*0xA0*/ NDC,'~','s','t','u','v','w','x', 'y','z',NDC,NDC,NDC,'[',NDC,NDC,
/*0xB0*/ NDC,NDC,NDC,NDC,NDC,NDC,NDC,NDC, NDC,NDC,NDC,NDC,NDC,']',NDC,NDC,
/*0xC0*/ '{','A','B','C','D','E','F','G', 'H','I',NDC,NDC,NDC,NDC,NDC,NDC,
/*0xD0*/ '}','J','K','L','M','N','O','P', 'Q','R',NDC,NDC,NDC,NDC,NDC,NDC,
/*0xE0*/ '\\',NDC,'S','T','U','V','W','X', 'Y','Z',NDC,NDC,NDC,NDC,NDC,NDC,
/*0xF0*/ '0','1','2','3','4','5','6','7', '8','9',NDC,NDC,NDC,NDC,NDC,NDC
};

void dumpinit(dumpopts_t *opts)
{
    if (opts == NULL) return;
    opts->bytesPerLine = 16;
    opts->groupSize    = 4;
    opts->groupGap     = 1;
    opts->indent       = 0;
    opts->offsetWidth  = 6;
    opts->offsetHex    = true;
#if defined(__MVS__)
    opts->showAscii    = false;     /* on z/OS, default to EBCDIC display */
    opts->showEbcdic   = true;
#else
    opts->showAscii    = true;
    opts->showEbcdic   = false;
#endif
}

int dumpwidth(uint64_t maxOffset, bool hex)
{
    int base = hex ? 16 : 10;
    int d = 1;
    while (maxOffset >= (uint64_t)base) { maxOffset /= (uint64_t)base; d++; }
    return d;
}

void dump(const uint8_t *buffer, uint64_t length, uint64_t startOffset,
          FILE *out, const dumpopts_t *opts)
{
    int    bpl, gs, gap, numGroups, j, k;
    size_t hexW, contentCap, p;
    char  *content, *prev;
    bool   havePrev = false;
    bool   runMarked = false;
    uint64_t i;

    if (buffer == NULL || out == NULL || opts == NULL || length == 0) return;

    bpl = opts->bytesPerLine;
    gs  = (opts->groupSize > 0) ? opts->groupSize : bpl;
    gap = (opts->groupGap  >= 0) ? opts->groupGap : 0;
    if (bpl < 1) return;

    numGroups  = (bpl + gs - 1) / gs;
    hexW       = (size_t)bpl * 2 + (size_t)gap * (numGroups > 0 ? numGroups - 1 : 0);
    contentCap = hexW
               + (opts->showAscii  ? (size_t)bpl + 3 : 0)
               + (opts->showEbcdic ? (size_t)bpl + 3 : 0)
               + 2;

    content = (char *)malloc(contentCap);
    prev    = (char *)malloc(contentCap);
    if (content == NULL || prev == NULL) {
        free(content); free(prev);
        return;
    }
    prev[0] = '\0';

    for (i = 0; i < length; i += bpl) {
        int  n      = (length - i < (uint64_t)bpl) ? (int)(length - i) : bpl;
        bool isLast = (i + (uint64_t)bpl >= length);

        /* ---- hex area (fixed width, padded for a short final row) ----- */
        p = 0;
        for (j = 0; j < bpl; j++) {
            if (j > 0 && j % gs == 0)
                for (k = 0; k < gap; k++) content[p++] = ' ';
            if (j < n) {
                content[p++] = hexdig[buffer[i + j] >> 4];
                content[p++] = hexdig[buffer[i + j] & 0x0F];
            } else {
                content[p++] = ' ';
                content[p++] = ' ';
            }
        }
        /* ---- ASCII column --------------------------------------------- */
        if (opts->showAscii) {
            content[p++] = ' ';
            content[p++] = '|';
            for (j = 0; j < bpl; j++)
                content[p++] = (j < n) ? (char)asctab[buffer[i + j]] : ' ';
            content[p++] = '|';
        }
        /* ---- EBCDIC (IBM-1047) column --------------------------------- */
        if (opts->showEbcdic) {
            content[p++] = ' ';
            content[p++] = '|';
            for (j = 0; j < bpl; j++)
                content[p++] = (j < n) ? (char)ebctab[buffer[i + j]] : ' ';
            content[p++] = '|';
        }
        content[p] = '\0';

        /* ---- collapse runs of identical lines ------------------------- */
        if (havePrev && !isLast && strcmp(content, prev) == 0) {
            if (!runMarked) {
                fprintf(out, "%*s*\n", opts->indent, "");
                runMarked = true;
            }
            continue;
        }
        runMarked = false;

        if (opts->offsetHex)
            fprintf(out, "%*s%0*llX:  %s\n", opts->indent, "",
                    opts->offsetWidth, (unsigned long long)(startOffset + i), content);
        else
            fprintf(out, "%*s%0*llu:  %s\n", opts->indent, "",
                    opts->offsetWidth, (unsigned long long)(startOffset + i), content);

        memcpy(prev, content, p + 1);
        havePrev = true;
    }

    free(content);
    free(prev);
}
