/**
 * @file md2.h
 * @brief MD2 model loading, animation and rendering.
 *
 * MD2 is Quake II's model format. Original loader by David HENRY, converted to
 * fixed point and heavily extended by smea (see the notice at the top of
 * md2.c). Every animated object in the game - the player's gun, turrets,
 * cubes, doors - is an MD2.
 *
 * @par Why MD2 suits the DS
 * MD2 stores a complete vertex snapshot per keyframe and animates by
 * interpolating between two of them. There is no skeleton and no per-vertex
 * weighting, so playback costs one lerp per vertex and nothing else - which is
 * about all a 67MHz CPU with no FPU can afford. Vertices are stored as bytes
 * plus a per-frame scale and translate, which also keeps the models small.
 *
 * @par Display lists
 * Feeding the geometry engine vertex by vertex is far too slow, so
 * @ref generateModelDisplayLists pre-bakes each frame into a hardware display
 * list that can be fired off with a single DMA. That is why a model's frames
 * carry a @c displayList array: the memory cost buys back a large amount of
 * per-frame CPU.
 *
 * @par Models versus instances
 * A @ref md2Model_struct is the shared, read-only asset. A
 * @ref modelInstance_struct is one thing in the world playing that asset: it
 * holds only the current animation and frame, so many instances share one
 * model.
 *
 * @see anorms.h and anorms2.h for the normal table MD2's packed normal indices
 *      refer to.
 */

#ifndef __MD29__
#define __MD29__

/* Vector */
typedef float vec3_t[3]; /**< @brief Float triplet, as it appears in the on-disk format. */

/**
 * @brief The on-disk MD2 header: counts and byte offsets for each section.
 */
/* MD2 header */
typedef struct
{
	int ident;   /**< Magic number, "IDP2". */
	int version; /**< Format version, always 8. */

	int skinwidth;  /**< Width of the skin the texture coordinates are relative to. */
	int skinheight; /**< Height of the skin. */

	int framesize; /**< Size of one frame record in bytes. */

	int num_skins;    /**< Number of skin names. */
	int num_vertices; /**< Vertices per frame; the same for every frame. */
	int num_st;       /**< Number of texture coordinate pairs. */
	int num_tris;     /**< Number of triangles. */
	int num_glcmds;   /**< Size of the OpenGL command list; unused here. */
	int num_frames;   /**< Number of keyframes. */

	int offset_skins;  /**< File offset of the skin names. */
	int offset_st;     /**< File offset of the texture coordinates. */
	int offset_tris;   /**< File offset of the triangles. */
	int offset_frames; /**< File offset of the frames. */
	int offset_glcmds; /**< File offset of the OpenGL command list. */
	int offset_end;    /**< File size. */
}md2_header_t;

/* Texture name */
typedef struct
{
	char name[64]; /**< Skin file name; the game supplies its own texture instead. */
}md2_skin_t;

/* Texture coords */
typedef struct
{
	short s; /**< Horizontal coordinate, in skin pixels. */
	short t; /**< Vertical coordinate, in skin pixels. */
}md2_texCoord_t;

/**
 * @brief One triangle: three vertex indices and three texture coordinate indices.
 *
 * The two index sets are separate because a vertex at a UV seam needs one
 * position but several texture coordinates.
 */
/* Triangle info */
typedef struct
{
	unsigned short vertex[3]; /**< Indices into the frame's vertex array. */
	unsigned short st[3];     /**< Indices into the model's texture coordinate array. */
}md2_triangle_t;

/**
 * @brief A vertex as stored on disk: three bytes plus a normal index.
 *
 * The bytes are expanded to real coordinates with the owning frame's
 * md2_frame_t::scale and md2_frame_t::translate.
 */
/* Compressed vertex */
typedef struct
{
	unsigned char v[3];        /**< Quantised position within the frame's bounding box. */
	unsigned char normalIndex; /**< Index into the shared normal table; see anorms.h. */
}md2_vertex_t;

/**
 * @brief One keyframe, plus everything precomputed from it at load time.
 */
/* Model frame */
typedef struct
{
	vect3D scale;     /**< Multiplier turning md2_vertex_t::v into model space. */
	vect3D translate; /**< Offset applied after ::scale. */
	char name[16];    /**< Frame name; the animation splitter parses these. */
	md2_vertex_t *verts; /**< The raw on-disk vertices. */
	vect3D* faceNormals; /**< One normal per triangle, computed at load time. */
	vect3D min, max;     /**< Bounding box of this frame. */
	u16 next;            /**< Frame this one interpolates towards. */
	u32* displayList[4]; /**< Pre-baked display lists; one per lighting/normal variant. */
}md2_frame_t;

/**
 * @brief A named range of frames.
 *
 * Built at load time by grouping frames whose names share a prefix, which is
 * the convention MD2 uses to pack several animations into one file.
 */
typedef struct
{
	u16 start; /**< First frame of the animation. */
	u16 end;   /**< Last frame of the animation. */
}md2Anim_struct;

/**
 * @brief A loaded model: the shared, read-only asset.
 */
/* MD2 model structure */
typedef struct
{
	md2_header_t header; /**< The on-disk header. */

	u8 numAnim;                  /**< Number of animations found in the frame names. */
	md2Anim_struct* animations;  /**< The animations themselves. */

	mtlImg_struct* texture; /**< Texture to bind before drawing. */

	md2_skin_t *skins;           /**< Skin names from the file; unused at runtime. */
	md2_texCoord_t *texcoords;   /**< Texture coordinates in skin pixels. */
	u32 *packedTexcoords;        /**< Texture coordinates packed for the geometry engine. */
	md2_triangle_t *triangles;   /**< The triangle list. */
	md2_frame_t *frames;         /**< The keyframes. */
}md2Model_struct;

/**
 * @brief One thing in the world playing a model.
 *
 * Deliberately small: many instances share a single @ref md2Model_struct, so
 * this holds only playback state.
 */
typedef struct
{
	u16 currentFrame;  /**< Frame currently being displayed. */
	u16 nextFrame;     /**< Frame being interpolated towards. */
	u16 interpCounter; /**< Sub-frame counter; a frame lasts four ticks. */
	u8 currentAnim;    /**< Animation currently playing. */
	u8 oldAnim;        /**< Animation to return to when a one-shot finishes. */
	bool oneshot;      /**< True while playing an animation that must run to completion. */
	u32* palette;      /**< Palette override, for tinting an instance differently. */
	md2Model_struct* model; /**< The shared asset being played. */
}modelInstance_struct;

/**
 * @brief Loads a model and its texture.
 * @param filename model file name.
 * @param texname  texture file name.
 * @param mdl      model to fill in.
 * @return non-zero on success.
 */
int loadMd2Model (const char *filename, char *texname, md2Model_struct *mdl);

/** @brief Releases everything a model owns. */
void freeMd2Model(md2Model_struct *mdl);

/**
 * @brief Draws a model interpolated between two frames.
 * @param n      first frame.
 * @param n2     second frame.
 * @param m      blend factor between them.
 * @param mdl    model to draw.
 * @param params polygon attributes to set before drawing.
 * @param center true to draw about the model's centre rather than its origin.
 * @param pal    palette override, or NULL for the model's own.
 * @param color  vertex colour to tint with.
 */
void renderModelFrameInterp(int n, int n2, int m, const md2Model_struct *mdl, u32 params, bool center, u32* pal, u16 color);

/**
 * @brief Pre-bakes every frame into a hardware display list.
 *
 * Trades RAM for a large drop in per-frame CPU cost. Call once after loading.
 *
 * @param mdl     model to prepare.
 * @param interp  true to also build the lists used for interpolated playback.
 * @param normals which normal variant to bake in.
 */
void generateModelDisplayLists(md2Model_struct *mdl, bool interp, u8 normals);

/** @brief Points an instance at a model and resets its playback state. */
void initModelInstance(modelInstance_struct* mi, md2Model_struct* mdl);

/**
 * @brief Switches an instance to another animation.
 *
 * A one-shot animation cannot be interrupted by a looping one: the request is
 * remembered as the animation to return to once the one-shot finishes.
 *
 * @param mi      instance to change.
 * @param newAnim animation index.
 * @param oneshot true to play once and then revert, false to loop.
 */
void changeAnimation(modelInstance_struct* mi, u16 newAnim, bool oneshot);

/** @brief Advances an instance's playback by one tick. Call once per frame. */
void updateAnimation(modelInstance_struct* mi);

#endif
