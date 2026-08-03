/**
 * @file pcx.h
 * @brief PCX image loader.
 *
 * Original code by David HENRY (see the acknowledgements in the README). All
 * of the game's 2D artwork - splash screens, menu backgrounds, model skins,
 * fonts - is stored as 8 bit palettised PCX, which is a good fit for the DS:
 * the format is trivially RLE compressed, and its indexed data maps straight
 * onto the hardware's palettised texture formats without conversion.
 *
 * @ref ReadPCXFile returns a @ref gl_texture_t holding the indices and palette;
 * @ref convertPCX16Bit expands that into direct 15 bit colour for the cases
 * that need a raw bitmap rather than a texture.
 *
 * @see textures.h for uploading the result into VRAM.
 */

#ifndef __PCX9__
#define __PCX9__

/**
 * @brief A decoded image, before it has been uploaded to VRAM.
 *
 * Either ::texels (8 bit indices plus ::palette) or ::texels16 (direct 15 bit
 * colour) is populated, depending on whether @ref convertPCX16Bit has run.
 */
/* OpenGL texture info */
struct gl_texture_t
{
	u16 width;  /**< Image width in pixels. */
	u16 height; /**< Image height in pixels. */

	int format;         /**< Source pixel format; carried over from the loader's OpenGL heritage. */
	int internalFormat; /**< Destination pixel format; unused on the DS. */
	u32 id;             /**< Texture handle; unused on the DS. */

	u8 *texels;     /**< 8 bit palette indices, one byte per pixel. */
	u16 *texels16;  /**< Direct 15 bit colour, filled in by @ref convertPCX16Bit. */
	u16 *palette;   /**< Palette, in the DS's 15 bit BGR format. */
};

#pragma pack(push, 1)
/**
 * @brief The on-disk PCX header.
 *
 * Packed to one-byte alignment so it can be read straight off the file. The
 * loader only supports the 8 bit RLE variant, which is what @c manufacturer
 * 0x0A / @c bitsPerPixel 8 identifies.
 *
 * @note push/pop rather than a bare @c pack(1) ... @c pack(4). The old pair
 *       did not restore the previous packing, it *set* packing to 4 for
 *       everything included after this header - which is most of the ARM9,
 *       since general.h pulls this in near the top. That was invisible on the
 *       DS, where 4 is both the default and the widest alignment anything
 *       needs, and wrong everywhere else: the host test build ended up with
 *       eight byte pointers on four byte boundaries.
 */
/* PCX header */
struct pcx_header_t
{
	u8 manufacturer; /**< Always 0x0A for a real PCX. */
	u8 version;      /**< Format revision. */
	u8 encoding;     /**< 1 for RLE, the only value supported. */
	u8 bitsPerPixel; /**< Bits per pixel per plane. */

	u16 xmin, ymin;         /**< Top left of the image window. */
	u16 xmax, ymax;         /**< Bottom right of the image window; the size is derived from these. */
	u16 horzRes, vertRes;   /**< Source device resolution; ignored. */

	u8 palette[48];    /**< 16 colour EGA palette; unused for 8 bit images, which store their palette at the end of the file. */
	u8 reserved;       /**< Padding. */
	u8 numColorPlanes; /**< Number of colour planes; 1 for palettised images. */

	u16 bytesPerScanLine; /**< Stride of one decoded scanline, which may exceed the width. */
	u16 paletteType;      /**< Colour or greyscale marker; ignored. */
	u16 horzSize, vertSize; /**< Intended display size; ignored. */

	u8 padding[54]; /**< Reserved, brings the header to 128 bytes. */
};
#pragma pack(pop)

/**
 * @brief Loads and decodes a PCX file.
 * @param filename  file name.
 * @param directory directory to look in; may be empty for the current one.
 * @return a newly allocated image, or NULL on failure. Free with @ref freePCX.
 */
struct gl_texture_t * ReadPCXFile (const char *filename, char* directory);

/**
 * @brief Expands a palettised image into direct 15 bit colour.
 *
 * Fills gl_texture_t::texels16. Used where the image is displayed as a raw
 * bitmap background rather than as a texture.
 */
void convertPCX16Bit(struct gl_texture_t* pcx);

/** @brief Releases an image returned by @ref ReadPCXFile. */
void freePCX(struct gl_texture_t * pcx);

#endif
