/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// tr_image.c

#include "tr_local.h"
#include "../rd-common/tr_common.h"

#include <map>

/*
** R_GammaCorrect
*/
void R_GammaCorrect( byte *buffer, int bufSize ) {
	int i;

	if ( vk.capture.image != VK_NULL_HANDLE )
		return;

	for ( i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

// makeup a nice clean, consistant name to query for and file under, for map<> usage...
//
char *GenerateImageMappingName( const char *name )
{
	static char sName[MAX_QPATH];
	int		i=0;
	char	letter;

	while (name[i] != '\0' && i<MAX_QPATH-1)
	{
		letter = tolower((unsigned char)name[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		sName[i++] = letter;
	}
	sName[i]=0;

	return &sName[0];
}

static float R_BytesPerTex (int format)
{
	switch ( format ) {
	case 1:
		//"I    "
		return 1;
		break;
	case 2:
		//"IA   "
		return 2;
		break;
	case 3:
		//"RGB  "
		return glConfig.colorBits/8.0f;
		break;
	case 4:
		//"RGBA "
		return glConfig.colorBits/8.0f;
		break;

	case GL_RGBA4:
		//"RGBA4"
		return 2;
		break;
	case GL_RGB5:
		//"RGB5 "
		return 2;
		break;

	case GL_RGBA8:
		//"RGBA8"
		return 4;
		break;
	case GL_RGB8:
		//"RGB8"
		return 4;
		break;

	case GL_RGB4_S3TC:
		//"S3TC "
		return 0.33333f;
		break;
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		//"DXT1 "
		return 0.33333f;
		break;
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		//"DXT5 "
		return 1;
		break;
	default:
		//"???? "
		return 4;
	}
}

/*
===============
R_SumOfUsedImages
===============
*/
float R_SumOfUsedImages( qboolean bUseFormat )
{
	int	total = 0;
	image_t *pImage;
#if 0
					  R_Images_StartIteration();
	while ( (pImage = R_Images_GetNextIteration()) != NULL)
	{
		if ( pImage->frameUsed == tr.frameCount- 1 ) {//it has already been advanced for the next frame, so...
			if (bUseFormat)
			{
				float  bytePerTex = R_BytesPerTex (pImage->internalFormat);
				total += bytePerTex * (pImage->width * pImage->height);
			}
			else
			{
				total += pImage->width * pImage->height;
			}
		}
	}
#endif
	return total;
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void ) {
	const image_t *image;
	int i, estTotalSize = 0;

	ri.Printf( PRINT_ALL, "\n -n- --w-- --h-- type  -size- mipmap --name-------\n" );

	for ( i = 0; i < tr.images.count; i++ )
	{
		const char *yesno[] = {"no ", "yes"};
		const char *format = "???? ";
		const char *sizeSuffix;
		int estSize;
		int displaySize;

		image = tr.images.items[i];
		estSize = image->uploadHeight * image->uploadWidth;

		switch ( image->internalFormat )
		{
			case VK_FORMAT_BC3_UNORM_BLOCK:
				format = "RGBA ";
				break;
			case VK_FORMAT_B8G8R8A8_UNORM:
				format = "BGRA ";
				estSize *= 4;
				break;
			case VK_FORMAT_R8G8B8A8_UNORM:
				format = "RGBA ";
				estSize *= 4;
				break;
			case VK_FORMAT_R8G8B8_UNORM:
				format = "RGB  ";
				estSize *= 3;
				break;
			case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
				format = "RGBA ";
				estSize *= 2;
				break;
			case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
				format = "RGB  ";
				estSize *= 2;
				break;
		}

		// mipmap adds about 50%
		if (image->flags & IMGFLAG_MIPMAP)
			estSize += estSize / 2;

		sizeSuffix = "b ";
		displaySize = estSize;

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "kb";
		}

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "Mb";
		}

		if ( displaySize >= 2048 )
		{
			displaySize = ( displaySize + 1023 ) / 1024;
			sizeSuffix = "Gb";
		}

		ri.Printf( PRINT_ALL, " %3i %5i %5i %s %4i%s %s %s\n", i, image->uploadWidth, image->uploadHeight, format, displaySize, sizeSuffix, yesno[(int)image->mipmap], image->imgName );
		estTotalSize += estSize;
	}

	ri.Printf( PRINT_ALL, " -----------------------\n" );
	ri.Printf( PRINT_ALL, " approx %i kbytes\n", (estTotalSize + 1023) / 1024 );
	ri.Printf( PRINT_ALL, " %i total images\n\n", tr.images.count );
}

/*
===============
RE_RegisterImages_Info_f

Lists the registered images with their upload size.
===============
*/
void RE_RegisterImages_Info_f( void ) {
	double texels = 0;
	uint32_t i;

	for ( i = 0; i < tr.images.count; i++ )
	{
		const image_t *image = tr.images.items[i];

		ri.Printf( PRINT_ALL, "%d: (%4dx%4dy) \"%s\"\n", i, image->uploadWidth, image->uploadHeight, image->imgName ? image->imgName : "" );
		texels += (double)image->uploadWidth * image->uploadHeight;
	}

	ri.Printf( PRINT_ALL, "%d Images. %.0f (%.2fMB) texels total, (not including mipmaps)\n", tr.images.count, texels, texels / 1024.0 / 1024.0 );
}

/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable( void ) {
	int		i;
	float	d;
	float	exp;

	exp = 0.5;

	for ( i = 0 ; i < FOG_TABLE_SIZE ; i++ ) {
		d = pow ( (float)i/(FOG_TABLE_SIZE-1), exp );

		tr.fogTable[i] = d;
	}
}

/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float	R_FogFactor( float s, float t ) {
	float	d;

	s -= 1.0/512;
	if ( s < 0 ) {
		return 0;
	}
	if ( t < 1.0/32 ) {
		return 0;
	}
	if ( t < 31.0/32 ) {
		s *= (t - 1.0f/32.0f) / (30.0f/32.0f);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if ( s > 1.0 ) {
		s = 1.0;
	}

	d = tr.fogTable[(uint32_t)(s * (FOG_TABLE_SIZE - 1))];

	return d;
}

/*
===============
RE_TempRawImage_ReadFromFile

Ported from rd-vanilla (tr_draw.cpp). Gives raw RGBA pixels of an image file
to the client or server. With pbReSampleBuffer, *piWidth and *piHeight are
also inputs and the image is scaled into that buffer. Always use the return
value. Call RE_TempRawImage_CleanUp afterwards.
===============
*/
static byte *s_pbTempRawImage = NULL;

void RE_TempRawImage_CleanUp( void )
{
	if ( s_pbTempRawImage )
	{
		R_Free( s_pbTempRawImage );
		s_pbTempRawImage = NULL;
	}
}

static byte *RE_TempRawImage_ReSample( byte *pbLoadedPic, int iLoadedWidth, int iLoadedHeight,
									   byte *pbReSampleBuffer, int *piWidth, int *piHeight )
{
	if ( pbReSampleBuffer == NULL || ( iLoadedWidth == *piWidth && iLoadedHeight == *piHeight ) )
	{
		*piWidth = iLoadedWidth;
		*piHeight = iLoadedHeight;
		return pbLoadedPic;
	}

	const float	fXStep = (float)iLoadedWidth / (float)*piWidth;
	const float	fYStep = (float)iLoadedHeight / (float)*piHeight;
	const int	iTotPixelsPerDownSample = (int)ceil( fXStep ) * (int)ceil( fYStep );
	byte		*pbDst = pbReSampleBuffer;

	for ( int y = 0; y < *piHeight; y++ )
	{
		for ( int x = 0; x < *piWidth; x++ )
		{
			int r = 0, g = 0, b = 0;

			for ( float yy = (float)y * fYStep; yy < (float)( y + 1 ) * fYStep; yy += 1 )
			{
				for ( float xx = (float)x * fXStep; xx < (float)( x + 1 ) * fXStep; xx += 1 )
				{
					const byte *pbSrc = pbLoadedPic + 4 * ( ( (int)yy * iLoadedWidth ) + (int)xx );
					r += pbSrc[0];
					g += pbSrc[1];
					b += pbSrc[2];
				}
			}

			pbDst[0] = r / iTotPixelsPerDownSample;
			pbDst[1] = g / iTotPixelsPerDownSample;
			pbDst[2] = b / iTotPixelsPerDownSample;
			pbDst[3] = 255;
			pbDst += 4;
		}
	}

	return pbReSampleBuffer;
}

byte *RE_TempRawImage_ReadFromFile( const char *psLocalFilename, int *piWidth, int *piHeight, byte *pbReSampleBuffer, qboolean qbVertFlip )
{
	RE_TempRawImage_CleanUp();

	byte *pbReturn = NULL;

	if ( psLocalFilename && piWidth && piHeight )
	{
		int iLoadedWidth, iLoadedHeight;

		R_LoadImage( psLocalFilename, &s_pbTempRawImage, &iLoadedWidth, &iLoadedHeight );
		if ( s_pbTempRawImage )
		{
			pbReturn = RE_TempRawImage_ReSample( s_pbTempRawImage, iLoadedWidth, iLoadedHeight,
												 pbReSampleBuffer, piWidth, piHeight );
		}
	}

	// Some callers expect OpenGL's bottom-up row order.
	if ( pbReturn && qbVertFlip )
	{
		unsigned int *pSrcLine = (unsigned int *)pbReturn;
		unsigned int *pDstLine = (unsigned int *)pbReturn + ( *piHeight * *piWidth ) - *piWidth;

		for ( int iLineCount = 0; iLineCount < *piHeight / 2; iLineCount++ )
		{
			for ( int x = 0; x < *piWidth; x++ )
			{
				const unsigned int l = pSrcLine[x];
				pSrcLine[x] = pDstLine[x];
				pDstLine[x] = l;
			}
			pSrcLine += *piWidth;
			pDstLine -= *piWidth;
		}
	}

	return pbReturn;
}
