#ifndef PKG_LIMITS_H
#define PKG_LIMITS_H
/* What one package may cost, in one place. Every download and every
 * extraction is checked against these; an archive over any of them is
 * refused before a byte is written.
 *
 * The limits are a guard against a damaged catalogue or server, not a budget:
 * archives are downloaded to a file and ZIP members inflate through fixed
 * 64 KiB buffers, so neither decides how much memory an install takes. Up to
 * 0.8.5 they were 64 MiB for the download, 128 MiB for a ZIP member, 64 MiB
 * for an LHA member and 256 MiB in all; an entry over any of those names the
 * client feature "large-archives" (tools/mkindex.py).
 *
 * The LHA reader holds the archive and one member in memory, so an LHA
 * archive and its members stay at 256 MiB each. */
#define PKG_MAX_DOWNLOAD        (1024L * 1024 * 1024) /* one archive, as downloaded */
#define PKG_MAX_MEMBER          (1024L * 1024 * 1024) /* one file, unpacked (ZIP) */
#define PKG_MAX_MEMBER_BUFFERED (256L * 1024 * 1024)  /* an LHA archive, and one file of it unpacked */
#define PKG_MAX_TOTAL           (2048L * 1024 * 1024) /* all files of one archive, unpacked */
/* The total does not fit a 32-bit long. */
typedef char pkg_limits_need_a_64_bit_long[sizeof(long) >= 8 ? 1 : -1];
#endif
