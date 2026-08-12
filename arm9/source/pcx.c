/**
 * @file pcx.c
 * @brief PCX image decoding.
 *
 * Implements @ref pcx.h. David HENRY's loader, handling the 8 bit palettised
 * RLE variant of PCX - which is every image in the game.
 *
 * The decoder is a straightforward run-length expansion: a byte with its top
 * two bits set is a run count followed by the value to repeat, anything else
 * is a literal. The palette lives in the last 769 bytes of the file, after a
 * 0x0C marker, and is converted from 24 bit RGB to the DS's 15 bit BGR here.
 *
 * @ref convertPCX16Bit expands the indexed result into direct colour for the
 * cases that need a raw bitmap - the splash screens and the editor's
 * screenshots - rather than a palettised texture.
 */

/*
* pcx.c -- pcx texture loader
* last modification: aug. 14, 2007
*
* Copyright (c) 2005-2007 David HENRY
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
* ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
* gcc -Wall -ansi -lGL -lGLU -lglut pcx.c -o pcx
*/

#include "game/game_main.h"

static int fileptr, filesize;
/*
static void
ReadPCX1bit (FILE *fp, const struct pcx_header_t *hdr, struct gl_texture_t *texinfo)
{
	int y, i, bytes;
	int colorIndex;
	int rle_count = 0, rle_value = 0;
	u8 *ptr = texinfo->texels;

	for (y = 0; y < texinfo->height; ++y)
	{
		ptr = &texinfo->texels[(texinfo->height - (y + 1)) * texinfo->width * 3];
		bytes = hdr->bytesPerScanLine;
*/
/* Decode line number y */
/*		while (bytes--)
		{
			if (rle_count == 0)
			{
				if ( (rle_value = fgetc (fp)) < 0xc0)
				{
					rle_count = 1;
				}
				else
				{
					rle_count = rle_value - 0xc0;
					rle_value = fgetc (fp);
				}
			}

			rle_count--;

			for (i = 7; i >= 0; --i, ptr += 3)
			{
				colorIndex = ((rle_value & (1 << i)) > 0);

				ptr[0] = hdr->palette[colorIndex * 3 + 0];
				ptr[1] = hdr->palette[colorIndex * 3 + 1];
				ptr[2] = hdr->palette[colorIndex * 3 + 2];
			}
		}
	}
}*/

static void
ReadPCX4bits (u8 *buffer, const struct pcx_header_t *hdr, struct gl_texture_t *texinfo)
{
	u8 *colorIndex, *line;
	u8 *pLine, *ptr;
	int rle_count = 0, rle_value = 0;
	int x, y, c, i;
	int bytes;

	colorIndex = (u8 *)malloc (sizeof (u8) * texinfo->width);
	line = (u8 *)malloc (sizeof (u8) * hdr->bytesPerScanLine);

	/* The plane loop below reads line[x/8] for every x up to the width, so a
	 * header claiming a stride narrower than that would read past the buffer
	 * it was just sized from. */
	if (!colorIndex || !line || hdr->bytesPerScanLine < (texinfo->width + 7) / 8)
	{
		free (colorIndex);
		free (line);
		free (texinfo->texels);
		texinfo->texels = NULL;
		return;
	}

/* Convert palette */
	for(i=0;i<16;i++)
	{
		texinfo->palette[i]=RGB15(hdr->palette[i*3+0]>>3,hdr->palette[i*3+1]>>3,hdr->palette[i*3+2]>>3);
	}

	for (y = 0; y < texinfo->height; ++y)
	{
// ptr = &texinfo->texels[(texinfo->height - (y + 1)) * texinfo->width * 3];
		ptr = &texinfo->texels[((y) * texinfo->width)/2];

		memset (colorIndex, 0, texinfo->width * sizeof (u8));

		for (c = 0; c < 4; ++c)
		{
			pLine = line;
			bytes = hdr->bytesPerScanLine;

/* Decode line number y */
			while (bytes--)
			{
				/* See the note in ReadPCX8bits for both guards. */
				if (rle_count <= 0)
				{
// if ( (rle_value = fgetc (fp)) < 0xc0)
					if (fileptr >= filesize) goto truncated;
					if ( (rle_value = buffer[fileptr++]) < 0xc0)
					{
						rle_count = 1;
					}
					else
					{
						rle_count = rle_value - 0xc0;
// rle_value = fgetc (fp);
						if (fileptr >= filesize) goto truncated;
						rle_value = buffer[fileptr++];
					}
				}

				rle_count--;
				*(pLine++) = rle_value;
			}

/* Compute line's color indexes */
			for (x = 0; x < texinfo->width; ++x)
			{
				if (line[x / 8] & (128 >> (x % 8)))colorIndex[x] += (1 << c);
			}
		}

/* Decode scan line.  color index => rgb  */
		int addr=0;
		for (x = 0; x < texinfo->width; ++x)
		{
// ptr[0] = hdr->palette[colorIndex[x] * 3 + 0];
// ptr[1] = hdr->palette[colorIndex[x] * 3 + 1];
// ptr[2] = hdr->palette[colorIndex[x] * 3 + 2];
			if((addr%2)){*ptr|=(colorIndex[x]&15)<<4;ptr++;}
			else {*ptr=(colorIndex[x]&15);}
			addr++;
		}
	}

/* Release memory */
	free (colorIndex);
	free (line);
	return;

truncated:
	/* As in ReadPCX8bits: a stream that ends mid-image is a failed load rather
	 * than a partly decoded one. */
	NOGBA("error: pixel data ended early\n");
	free (colorIndex);
	free (line);
	free (texinfo->texels);
	texinfo->texels = NULL;
}



static void
ReadPCX8bits (u8 *buffer, const struct pcx_header_t *hdr,
	struct gl_texture_t *texinfo)
{
	int rle_count = 0, rle_value = 0, i;

	u8 * palette=malloc(768 *sizeof(u8) );

	u8 magic;
	u8 *ptr;
	int curpos; //not fpos_t: this only ever holds fileptr, which is an index
	int y, bytes;

	if (!palette)
	{
		free (texinfo->texels);
		texinfo->texels = NULL;
		return;
	}

	/* The palette is contained in the last 769 bytes of the file */
	// fgetpos (fp, &curpos);
	curpos=fileptr;
	// fseek (fp, -769, SEEK_END);
	fileptr=filesize-769;

	/* ...so a file shorter than that has no palette to seek back to, and this
	 * used to index the buffer from a negative offset. */
	if (fileptr < 0)
	{
		NOGBA("error: file too short to hold a palette\n");
		free (texinfo->texels);
		texinfo->texels = NULL;
		free (palette);
		return;
	}

	// magic = fgetc (fp);
	magic=buffer[fileptr++];


	/* First byte must be equal to 0x0c (12) */
	if (magic != 0x0c)
	{
		NOGBA("error: colormap's first byte must be 0x0c! "
			"(%#x)\n", magic);

		free (texinfo->texels);
		texinfo->texels = NULL;
		free(palette);
		return;
	}

	/* Read palette */
	// fread (palette, sizeof (u8), 768, fp);
	memcpy(palette,&buffer[fileptr],768);

	/* Convert palette */
	for(i=0;i<256;i++)
	{
		texinfo->palette[i]=RGB15(palette[i*3+0]>>3,palette[i*3+1]>>3,palette[i*3+2]>>3);
	}

	// fsetpos (fp, &curpos);
	fileptr=curpos;

	/* Read pixel data */
	for (y = 0; y < texinfo->height; ++y)
	{
	// ptr = &texinfo->texels[(texinfo->height - (y + 1)) * texinfo->width * 3];
		ptr = &texinfo->texels[(y) * texinfo->width];
		bytes = hdr->bytesPerScanLine;

		/* A PCX stride is padded to an even byte count, so bytesPerScanLine is
		 * wider than the image for every odd width. The whole stride has to be
		 * decoded to stay in step with the stream, but only the first width
		 * bytes of it belong to the row - the padding used to be written past
		 * the end of the row, one byte per row for the whole image. */
		int written = 0;

	/* Decode line number y */
		while (bytes--)
		{
			/* <=, not ==: a 0xC0 byte is a run of length zero, which the
			 * decrement below takes to -1. An equality test never matches
			 * again after that, so the decoder stopped reading input
			 * altogether and painted the rest of the image with whatever
			 * value it happened to be holding. */
			if (rle_count <= 0)
			{
	// if( (rle_value = fgetc (fp)) < 0xc0)
				if (fileptr >= filesize) goto truncated;
				if( (rle_value = buffer[fileptr++]) < 0xc0)
				{
					rle_count = 1;
				}
				else
				{
					rle_count = rle_value - 0xc0;
	// rle_value = fgetc (fp);
					if (fileptr >= filesize) goto truncated;
					rle_value = buffer[fileptr++];
				}
			}

			rle_count--;

	// ptr[0] = palette[rle_value * 3 + 0];
	// ptr[1] = palette[rle_value * 3 + 1];
	// ptr[2] = palette[rle_value * 3 + 2];
			if (written < texinfo->width)
			{
				*ptr = rle_value;
	// ptr += 3;
				ptr++;
				written++;
			}
		}
	}
    free(palette);
    return;

truncated:
	/* The stream ran out mid-image. Report that as a failed load rather than
	 * handing back a half decoded texture: ReadPCXFile turns a NULL texels
	 * into a NULL return, which is the one thing every caller checks. */
	NOGBA("error: pixel data ended early\n");
	free (texinfo->texels);
	texinfo->texels = NULL;
	free (palette);
}

	/*static void
ReadPCX24bits (FILE *fp, const struct pcx_header_t *hdr,
struct gl_texture_t *texinfo)
{
u8 *ptr = texinfo->texels;
int rle_count = 0, rle_value = 0;
int y, c;
int bytes;

for (y = 0; y < texinfo->height; ++y)
{
for (c = 0; c < 3; ++c)
{
ptr = &texinfo->texels[(texinfo->height - (y + 1)) * texinfo->width * 3];
bytes = hdr->bytesPerScanLine;

while (bytes--)
{
if (rle_count == 0)
{
if( (rle_value = fgetc (fp)) < 0xc0)
{
rle_count = 1;
}
else
{
rle_count = rle_value - 0xc0;
rle_value = fgetc (fp);
}
}

rle_count--;
ptr[c] = (u8)rle_value;
ptr += 3;
}
}
}
}*/

void convertPCX16Bit(struct gl_texture_t* pcx)
{
	if(!pcx || !pcx->texels || !pcx->palette || pcx->format!=8)return;

	pcx->format=16;
	pcx->texels16=(u16*)malloc(sizeof(u16)*pcx->width*pcx->height);
	if(!pcx->texels16)return;

	int i, j;
	for(i=0;i<pcx->width;i++)
	{
		for(j=0;j<pcx->height;j++)
		{
			pcx->texels16[i+j*pcx->width]=pcx->palette[pcx->texels[i+j*pcx->width]]|((pcx->texels[i+j*pcx->width]==0&&pcx->palette[0]==RGB15(31,0,31))?0:(1<<15));
		}
	}

	free(pcx->texels);
	free(pcx->palette);
	pcx->texels = NULL;
	pcx->palette = NULL;
}

//extern int lastSize;


struct pcx_header_t header;
struct gl_texture_t * ReadPCXFile (const char *filename, char* directory)
{
	struct gl_texture_t *texinfo;
	int bitcount;
	u8* buffer;

/* Open image file */
// fp = fopen (filename, "rb");
// fp = DS_OpenFile(filename, directory, false, true);
	buffer = bufferizeFile((char*)filename, directory, NULL, true);
	filesize=lastSize;
	fileptr=0;
	if (!buffer)
	{
		//char path[255];
		//getcwd(path, 255);
		//NOGBA("error: couldn't open \"%s\"! (%s)\n", filename, path);
		return NULL;
	}

/* Read header file */
// fread (&header, sizeof (struct pcx_header_t), 1, fp);

	if (filesize < (int)sizeof (header))
	{
		NOGBA("error: \"%s\" is too small to hold a PCX header\n", filename);
		free (buffer);
		return NULL;
	}

	memcpy(&header,buffer,sizeof (header));
	fileptr+=sizeof (struct pcx_header_t);

	if (header.manufacturer != 0x0a)
	{
		NOGBA("error: bad version number! (%i)\n",
			header.manufacturer);
		free (buffer);
		return NULL;
	}

/* Initialize texture parameters */

	texinfo = (struct gl_texture_t *)malloc (sizeof (struct gl_texture_t));
	if (!texinfo)
	{
		free (buffer);
		return NULL;
	}
	/* The size is stored as two corners and taken as their difference. A
	 * reversed pair underflows the u16 it is kept in, which is how a garbage
	 * header turns into a 65000 pixel row. */
	if (header.xmax < header.xmin || header.ymax < header.ymin)
	{
		NOGBA("error: nonsensical image window\n");
		free (buffer);
		free (texinfo);
		return NULL;
	}

	texinfo->width = header.xmax - header.xmin + 1;
	texinfo->height = header.ymax - header.ymin + 1;
// texinfo->format = GL_RGB;
	texinfo->internalFormat = 3;

	bitcount = header.bitsPerPixel * header.numColorPlanes;
	texinfo->format = bitcount;
	texinfo->texels=NULL;
	texinfo->palette=NULL;
	texinfo->texels16=NULL;
/* Read image data */

	switch (bitcount)
	{
		/*case 1:
		ReadPCX1bit (fp, &header, texinfo);
		break;*/

		case 4:
		/* 4 bits color index */
		NOGBA("LOADING 4BIT");
		texinfo->texels = (u8 *) malloc ((sizeof (u8) * texinfo->width * texinfo->height) / 2);
		texinfo->palette = (u16 *) malloc (sizeof (u16) * 16);
		if (texinfo->texels && texinfo->palette)
			ReadPCX4bits (buffer, &header, texinfo);
		break;

		case 8:
		/* 8 bits color index */
		texinfo->texels = (u8 *) malloc (sizeof (u8) * texinfo->width * texinfo->height);
		texinfo->palette = (u16 *) malloc (sizeof (u16) * 256);
		NOGBA("TEXELS %p",texinfo->texels);
		if (texinfo->texels && texinfo->palette)
			ReadPCX8bits (buffer, &header, texinfo);
		break;

		/*case 24:
		ReadPCX24bits (fp, &header, texinfo);
		break;*/

		default:
		/* Unsupported */
		NOGBA("error: unknown %i bitcount pcx files\n", bitcount);
		break;
	}
	free(buffer);

	/*
	 * A single failure signal. This used to hand back a texture with a NULL
	 * texels pointer for an unsupported depth, a failed allocation or a bad
	 * palette marker - so a caller checking the return value for NULL, which
	 * is the only thing there is to check, still got something it could not
	 * use. Callers now only have to test the one pointer.
	 */
	if (!texinfo->texels || !texinfo->palette)
	{
		freePCX (texinfo);
		return NULL;
	}

	return texinfo;
}

void freePCX(struct gl_texture_t * pcx)
{
	if(!pcx)return;
	if(pcx->texels)free(pcx->texels);
	if(pcx->texels16)free(pcx->texels16);
	if(pcx->palette)free(pcx->palette);
	pcx->texels=NULL;
	pcx->texels16=NULL;
	pcx->palette=NULL;
	free(pcx);
}
