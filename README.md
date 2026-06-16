# filedump - Cross-platform z/OS-leaning hex dump utility (and library routine)

Executable for z/OS, Windows and Linux (x64) is provided.

Source can be compiled on any of these platforms, using an appropriate C compiler:
* On z/OS, IBM XL C/C++
* On Linux, GCC
* On Windows, Microsoft Visual C++

Execute the filedump utility with no parameters to see usage information:

```
$ filedump
Usage: filedump [options] file        (use "-" for standard input)

Display options:
  -w N   bytes per line               (default 16)
  -g N   bytes per hex group          (default 4)
  -s N   spaces between groups        (default 1)
  -i N   indent each line by N spaces (default 0)
  -d     decimal offsets              (default hex)
  -x     hex offsets                  (default)
  -a     show ASCII character column
  -e     show EBCDIC (IBM-1047) column
  -n     no character column (hex only)
         (column defaults to ASCII, or to EBCDIC when -v is used)

Input options:
  -v     variable-length records: each record is LLZZ + data,
         LL = 2-byte big-endian total length (incl. the 4-byte
         LLZZ); ZZ is shown but not interpreted (any platform)
  -l     with -v, include the 4-byte LLZZ in the dumped data
         (default dumps data only; LL/ZZ always appear in the
          record header line either way)
  -h     this help
```
