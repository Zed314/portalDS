/**
 * @file compress.h
 * @brief Run-length compression for level data.
 *
 * Taken from GRIT (http://www.coranac.com/projects/grit/) and adapted to work
 * on 16 bit units rather than bytes, because the level format stores block ids
 * as @c u16 and byte-wise RLE was leaving most of the redundancy on the table.
 *
 * The compressed stream keeps GBA/DS BIOS-style framing (a tag byte plus a
 * 24 bit decompressed size), so the header word identifies both the scheme and
 * the size to expect.
 *
 * @see compression.c, and editor/io.c for the level files this is used on.
 */

#ifndef COMPRESS_H
#define COMPRESS_H

/**
 * @brief Compresses a buffer of 16 bit values.
 *
 * Allocates the destination itself; the caller owns the result.
 *
 * @param dst  out: receives a newly allocated buffer holding the compressed stream.
 * @param srcD source data.
 * @param srcS size of @p srcD in bytes.
 * @return the size of the compressed stream in bytes, or 0 on failure.
 */
uint32_t compressRLE(u16 **dst, u16 *srcD, uint32_t srcS);

/**
 * @brief Expands a stream produced by @ref compressRLE.
 *
 * @param dst  destination buffer, which must already be large enough.
 * @param src  compressed stream.
 * @param dstS size of @p dst in bytes.
 * @return the number of bytes written.
 */
uint32_t decompressRLE(u16 *dst, u16 *src, uint32_t dstS);

#endif
