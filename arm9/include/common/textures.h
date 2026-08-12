/**
 * @file textures.h
 * @brief VRAM bank management and texture upload.
 *
 * The DS's texture memory is not a heap. It is a handful of fixed 128KB VRAM
 * banks that must be *unmapped* from the CPU before the 3D engine can read
 * them, so textures cannot simply be written wherever there is room: they have
 * to be packed into banks up front, and a bank can only be written to while it
 * is mapped in. This file is the allocator that hides all of that.
 *
 * @ref initVramBanks claims a number of banks, and each @c create* function
 * finds space in one of them, uploads the pixel data and fills in an
 * @ref mtlImg_struct describing where it landed. From then on drawing only
 * needs @ref bindTexture (or the inline @ref Game_FastBind, which skips the
 * bookkeeping and just pokes the two hardware registers).
 *
 * @par Texture formats in use
 * Two matter here:
 *  - **paletted**, for world and model textures - one byte per pixel plus a
 *    shared palette, which is what PCX artwork maps onto directly;
 *  - **A5I3**, five bits of alpha and a three colour palette, used for the
 *    portal outlines and other soft-edged overlays where a hard alpha mask
 *    would look wrong.
 *
 * @par Reserved textures
 * @ref createReservedTextureBufferA5I3 places a texture at a caller-chosen
 * address instead of letting the allocator pick. That is how the portal view
 * textures work: their contents are produced by the display capture unit
 * writing straight into VRAM, so the texture has to sit exactly where the
 * capture lands.
 *
 * @see pcx.h for where the pixel data comes from.
 */

#ifndef __TEXTURES9__
#define __TEXTURES9__

#define MAX_TEX 128       /**< Maximum number of live textures. */
#define BANKS vramBanks   /**< Number of VRAM banks currently claimed for textures. */

extern u8 vramBanks; /**< How many banks @ref initVramBanks took. */

/**
 * @brief Allocation state of one 128KB VRAM bank.
 */
typedef struct
{
	size_t s_total; /**< Bank capacity in bytes. */
	size_t s_used;  /**< Bytes allocated so far. */
	size_t s_free;  /**< Bytes still available. */
	void* addr;     /**< Base address of the bank while it is CPU-mapped. */
	int num_t;      /**< Number of textures living in this bank. */
}vramBank_struct;

/**
 * @brief A texture: where it lives in VRAM and how to bind it.
 */
typedef struct
{
	u16 width, height;   /**< Size in pixels as requested. */
	u16 rwidth, rheight; /**< Size actually allocated, rounded up to the powers of two the hardware requires. */
	int ID;              /**< Slot index in the texture table. */
	int bank;            /**< Which VRAM bank holds the pixel data. */
	size_t size;         /**< Bytes of VRAM the pixel data occupies. */
	u32 param;           /**< Pre-built value for @c GFX_TEX_FORMAT: address, size and format in one word. */
	char* name;          /**< Source file name, kept so a texture can be looked up by name. */
	void *addr;          /**< Address of the pixel data. */
	void *pal;           /**< Address of the palette. */
	u32 palbind;         /**< Pre-built value for @c GFX_PAL_FORMAT. */
	bool used;           /**< False when this slot is free. */
}mtlImg_struct;

/**
 * @brief Binds a texture by poking the two hardware registers directly.
 *
 * The fast path used in inner drawing loops - it skips every check
 * @ref bindTexture makes, so the texture must genuinely be resident.
 */
static inline void Game_FastBind(mtlImg_struct *mtl)
{
	GFX_PAL_FORMAT = mtl->palbind;
	GFX_TEX_FORMAT = mtl->param;
}

/** @brief Clears the texture table. Does not release VRAM banks. */
void initTextures();

/**
 * @brief Loads a PCX file into a new texture.
 * @param filename  image file name.
 * @param directory directory to look in.
 * @return the new texture, or NULL if it could not be loaded or did not fit.
 */
mtlImg_struct* createTexture(char* filename, char* directory);

/**
 * @brief Creates a direct 15 bit colour texture from a buffer.
 * @param buffer pixel data, 16 bits per pixel.
 * @param x      width in pixels.
 * @param y      height in pixels.
 * @param cpy    true to copy the data into VRAM now; false to only reserve the space.
 */
mtlImg_struct* createTextureBuffer16(u16* buffer, u16 x, u16 y, bool cpy);

/**
 * @brief Creates a paletted texture from buffers.
 * @param buffer  8 bit palette indices.
 * @param buffer2 palette, in 15 bit BGR.
 * @param x       width in pixels.
 * @param y       height in pixels.
 */
mtlImg_struct* createTextureBuffer(u8* buffer, u16* buffer2, u16 x, u16 y);

/**
 * @brief Creates an A5I3 texture: 5 bits of alpha, 3 bits of palette index.
 *
 * Used wherever a smooth alpha edge is needed - portal outlines, particle
 * sprites, the gun's glow.
 */
mtlImg_struct* createTextureBufferA5I3(u8* buffer, u16* buffer2, u16 x, u16 y);

/**
 * @brief Creates an A5I3 texture at a caller-chosen VRAM address.
 *
 * For textures whose contents are written by something other than the CPU -
 * chiefly the display capture unit, which produces the portal views.
 *
 * @param buffer  alpha and index data.
 * @param buffer2 palette, in 15 bit BGR.
 * @param x       width in pixels.
 * @param y       height in pixels.
 * @param addr    exact VRAM address to place the texture at.
 */
mtlImg_struct* createReservedTextureBufferA5I3(u8* buffer, u16* buffer2, u16 x, u16 y, void* addr);

/** @brief Copies pixel data into an already allocated texture's VRAM. */
void loadToBank(mtlImg_struct *mtl, u8* data);

/** @brief Copies a palette into an already allocated texture's palette memory. */
void loadPaletteToBank(mtlImg_struct *mtl, u16* data, size_t size);

/** @brief Uploads 15 bit colour data into an existing texture; @p genaddr allocates VRAM first. */
void loadTextureBuffer16(u16* buffer, u16 x, u16 y, mtlImg_struct *mtl, bool genaddr, bool cpy);

/** @brief Uploads paletted data into an existing texture. */
void loadTextureBuffer(u8* buffer, u16* buffer2, u16 x, u16 y, mtlImg_struct *mtl);

/** @brief Uploads A5I3 data into an existing texture. */
void loadTextureBufferA5I3(u8* buffer, u16* buffer2, u16 x, u16 y, mtlImg_struct *mtl);

/** @brief Uploads A5I3 data into an existing texture placed at a fixed address. */
void loadReservedTextureBufferA5I3(u8* buffer, u16* buffer2, u16 x, u16 y, mtlImg_struct *mtl, void* addr);

/**
 * @brief Uploads a sub-rectangle into a texture, optionally rotated 90 degrees.
 *
 * Used to build atlases, and to pack the tall thin portal view textures into
 * banks that are wider than they are tall.
 *
 * @param mtl  destination texture.
 * @param data pixel data for the sub-rectangle.
 * @param w    sub-rectangle width.
 * @param h    sub-rectangle height.
 * @param x    destination x within the texture.
 * @param y    destination y within the texture.
 * @param rot  true to rotate the data as it is written.
 */
void loadPartToBank(mtlImg_struct *mtl, u8* data, u16 w, u16 h, u16 x, u16 y, bool rot);

/** @brief Loads a PCX file into an already allocated texture. */
void loadTexturePCX(char* filename, char* directory, mtlImg_struct* mtl);

/** @brief Loads only the palette out of a PCX file. Caller owns the result. */
u32* loadPalettePCX(char* filename, char* directory);

/** @brief Overwrites one palette entry in place; used for the flashing indicator lights. */
void editPalette(u16* addr, u8 index, u16 color);

/** @brief Binds a texture and its palette, and sets the polygon attributes that go with it. */
void applyMTL(mtlImg_struct *mtl);

/**
 * @brief Claims VRAM banks for texture use and maps them for CPU writing.
 * @param banks number of banks to take.
 */
void initVramBanks(u8 banks);

/** @brief Reports per-bank usage to the debug output. */
void getVramStatus();

/** @brief Clears the currently bound texture. */
void unbindMtl();

/** @brief Copies pixel data into a specific bank. */
void addToBank(mtlImg_struct *mtl, u8* data, int b);

/** @brief Reads a texture's palette back out of VRAM. */
void getPaletteFromBank(mtlImg_struct *mtl, u16* data, size_t size);

/**
 * @brief Builds the mtlImg_struct::param word the hardware needs.
 * @param mtl   texture to configure.
 * @param sizeX width selector, as a power-of-two exponent.
 * @param sizeY height selector.
 * @param addr  VRAM address of the pixel data.
 * @param mode  texture format.
 * @param param extra attribute bits to OR in.
 */
void setTextureParameter(mtlImg_struct *mtl, uint8 sizeX, uint8 sizeY, const uint32* addr, GL_TEXTURE_TYPE_ENUM mode, uint32 param);

/** @brief Returns a texture's VRAM address. */
void* getTextureAddress(mtlImg_struct *mtl);

/** @brief Copies a palette into VRAM for a texture. */
void addPaletteToBank(mtlImg_struct *mtl, u16* data, size_t size);

/** @brief Binds a texture's palette. */
void bindPalette(mtlImg_struct *mtl);

/** @brief Binds a palette for a 4 colour texture format. */
void bindPalette4(mtlImg_struct *mtl);

/** @brief Binds a texture and its palette. */
void bindTexture(mtlImg_struct *mtl);

/** @brief Binds a palette by raw address, bypassing any mtlImg_struct. */
void bindPaletteAddr(u32* addr);

/**
 * @brief Converts pixel dimensions into the hardware's size selectors.
 *
 * The 3D engine only accepts power-of-two texture sizes expressed as
 * exponents, so this rounds up and returns the exponents.
 *
 * @param width  requested width.
 * @param height requested height.
 * @param w      out: width selector.
 * @param l      out: height selector.
 */
void getGlWL(u16 width, u16 height, u8* w, u8* l);

/**
 * @brief Re-describes an A5I3 texture at a different size without moving it.
 *
 * Used for the portal views, whose visible area changes as the portal is seen
 * more obliquely, but whose VRAM allocation must stay put.
 */
void changeTextureSizeA5I3(mtlImg_struct *mtl, u16 x, u16 y);

#endif
