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

#include "cg_headers.h"

#include "cg_media.h"

/*
================
CG_DrawSides

Coords are virtual 640x480
================
*/
void CG_DrawSides(float x, float y, float w, float h, float size) {
	//size *= cgs.screenXScale;
	cgi_R_DrawStretchPic( x, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
	cgi_R_DrawStretchPic( x + w - size, y, size, h, 0, 0, 0, 0, cgs.media.whiteShader );
}

void CG_DrawTopBottom(float x, float y, float w, float h, float size) {
	//size *= cgs.screenYScale;
	cgi_R_DrawStretchPic( x, y, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
	cgi_R_DrawStretchPic( x, y + h - size, w, size, 0, 0, 0, 0, cgs.media.whiteShader );
}

/*
================
CG_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void CG_DrawRect( float x, float y, float width, float height, float size, const float *color ) {
	cgi_R_SetColor( color );

	CG_DrawTopBottom(x, y, width, height, size);
	CG_DrawSides(x, y, width, height, size);

	cgi_R_SetColor( NULL );
}

/*
================
CG_FillRect

Coordinates are 640*480 virtual values
=================
*/
void CG_FillRect( float x, float y, float width, float height, const float *color ) {
	cgi_R_SetColor( color );
	cgi_R_DrawStretchPic( x, y, width, height, 0, 0, 0, 0, cgs.media.whiteShader);
	cgi_R_SetColor( NULL );
}


/*
================
CG_Scissor

Coordinates are 640*480 virtual values
=================
*/
void CG_Scissor( float x, float y, float width, float height)
{

	cgi_R_Scissor( x, y, width, height);

}


/*
================
CG_DrawPic

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
=================
*/
void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	cgi_R_DrawStretchPic( x, y, width, height, 0, 0, 1, 1, hShader );
}

/*
================
CG_DrawPic2

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
Can also specify the exact texture coordinates
=================
*/
void CG_DrawPic2( float x, float y, float width, float height, float s1, float t1, float s2, float t2, qhandle_t hShader )
{
	cgi_R_DrawStretchPic( x, y, width, height, s1, t1, s2, t2, hShader );
}

/*
================
CG_DrawRotatePic

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
rotates around the upper right corner of the passed in point
=================
*/
void CG_DrawRotatePic( float x, float y, float width, float height,float angle, qhandle_t hShader, float aspectCorrection) {
	cgi_R_DrawRotatePic( x, y, width, height, 0, 0, 1, 1, angle, hShader, aspectCorrection);
}

/*
================
CG_DrawRotatePic2

Coordinates are 640*480 virtual values
A width of 0 will draw with the original image width
Actually rotates around the center point of the passed in coordinates
=================
*/
void CG_DrawRotatePic2( float x, float y, float width, float height,float angle, qhandle_t hShader, float aspectCorrection) {
	cgi_R_DrawRotatePic2( x, y, width, height, 0, 0, 1, 1, angle, hShader, aspectCorrection);
}

/*
===============
CG_DrawChar

Coordinates and size in 640*480 virtual screen size
===============
*/
void CG_DrawChar( int x, int y, int width, int height, int ch ) {
	int row, col;
	float frow, fcol;
	float size;
	float	ax, ay, aw, ah;

	ch &= 255;

	if ( ch == ' ' ) {
		return;
	}

	ax = x;
	ay = y;
	aw = width;
	ah = height;

	row = ch>>4;
	col = ch&15;
/*
	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

	cgi_R_DrawStretchPic( ax, ay, aw, ah,
					   fcol, frow,
					   fcol + size, frow + size,
					   cgs.media.charsetShader );
*/

	float size2;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.03125;
	size2 = 0.0625;

	cgi_R_DrawStretchPic( ax, ay, aw, ah, fcol, frow, fcol + size, frow + size2,
		cgs.media.charsetShader );

}


/*
==================
CG_DrawStringExt

Draws a multi-colored string with a drop shadow, optionally forcing
to a fixed color.

Coordinates are at 640 by 480 virtual resolution
==================
*/
void CG_DrawStringExt( int x, int y, const char *string, const float *setColor,
		qboolean forceColor, qboolean shadow, int charWidth, int charHeight ) {
	vec4_t		color;
	const char	*s;
	int			xx;

	// draw the drop shadow
	if (shadow) {
		color[0] = color[1] = color[2] = 0;
		color[3] = setColor[3];
		cgi_R_SetColor( color );
		s = string;
		xx = x;
		while ( *s ) {
			if ( Q_IsColorString( s ) ) {
				s += 2;
				continue;
			}
			CG_DrawChar( xx + 2, y + 2, charWidth, charHeight, *s );
			xx += charWidth;
			s++;
		}
	}

	// draw the colored text
	s = string;
	xx = x;
	cgi_R_SetColor( setColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( !forceColor ) {
				memcpy( color, g_color_table[ColorIndex(*(s+1))], sizeof( color ) );
				color[3] = setColor[3];
				cgi_R_SetColor( color );
			}
			s += 2;
			continue;
		}
		CG_DrawChar( xx, y, charWidth, charHeight, *s );
		xx += charWidth;
		s++;
	}
	cgi_R_SetColor( NULL );
}


void CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color ) {
	CG_DrawStringExt( x, y, s, color, qtrue, qfalse, SMALLCHAR_WIDTH, SMALLCHAR_HEIGHT );
}

/*
=================
CG_DrawStrlen

Returns character count, skiping color escape codes
=================
*/
int CG_DrawStrlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/*
=============
CG_TileClearBox

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
static void CG_TileClearBox( int x, int y, int w, int h, qhandle_t hShader ) {
	float	s1, t1, s2, t2;

	s1 = x/64.0;
	t1 = y/64.0;
	s2 = (x+w)/64.0;
	t2 = (y+h)/64.0;
	cgi_R_DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader );
}



/*
==============
CG_TileClear

Clear around a sized down screen
==============
*/
void CG_TileClear( void ) {
	int		top, bottom, left, right;
	int		w, h;

	w = cgs.glconfig.vidWidth;
	h = cgs.glconfig.vidHeight;

	if ( cg.refdef.x == 0 && cg.refdef.y == 0 &&
		cg.refdef.width == w && cg.refdef.height == h ) {
		return;		// full screen rendering
	}

	top = cg.refdef.y;
	bottom = top + cg.refdef.height-1;
	left = cg.refdef.x;
	right = left + cg.refdef.width-1;

	// clear above view screen
	CG_TileClearBox( 0, 0, w, top, cgs.media.backTileShader );

	// clear below view screen
	CG_TileClearBox( 0, bottom, w, h - bottom, cgs.media.backTileShader );

	// clear left of view screen
	CG_TileClearBox( 0, top, left, bottom - top + 1, cgs.media.backTileShader );

	// clear right of view screen
	CG_TileClearBox( right, top, w - right, bottom - top + 1, cgs.media.backTileShader );
}



/*
================
CG_FadeColor
================
*/
float *CG_FadeColor( int startMsec, int totalMsec ) {
	static vec4_t		color;
	int			t;

	if ( startMsec == 0 ) {
		return NULL;
	}

	t = cg.time - startMsec;

	if ( t >= totalMsec ) {
		return NULL;
	}

	// fade out
	if ( totalMsec - t < FADE_TIME ) {
		color[3] = ( totalMsec - t ) * 1.0/FADE_TIME;
	} else {
		color[3] = 1.0;
	}
	color[0] = color[1] = color[2] = 1;

	return color;
}

/*
==============
CG_DrawNumField

Take x,y positions as if 640 x 480 and scales them to the proper resolution

==============
*/
void CG_DrawNumField (int x, int y, int width, int value,int charWidth,int charHeight,int style,qboolean zeroFill)
{
	char	num[16], *ptr;
	int		l;
	int		frame;
	int		xWidth;

	if (width < 1) {
		return;
	}

	// draw number string
	if (width > 5) {
		width = 5;
	}

	switch ( width ) {
	case 1:
		value = value > 9 ? 9 : value;
		value = value < 0 ? 0 : value;
		break;
	case 2:
		value = value > 99 ? 99 : value;
		value = value < -9 ? -9 : value;
		break;
	case 3:
		value = value > 999 ? 999 : value;
		value = value < -99 ? -99 : value;
		break;
	case 4:
		value = value > 9999 ? 9999 : value;
		value = value < -999 ? -999 : value;
		break;
	}

	Com_sprintf (num, sizeof(num), "%i", value);
	l = strlen(num);
	if (l > width)
		l = width;

	// FIXME: Might need to do something different for the chunky font??
	switch(style)
	{
	case NUM_FONT_SMALL:
		xWidth = charWidth;
		break;
	case NUM_FONT_CHUNKY:
		xWidth = (charWidth/1.2f) + 2;
		break;
	default:
	case NUM_FONT_BIG:
		xWidth = (charWidth/2) + 7;//(charWidth/6);
		break;
	}

	if ( zeroFill )
	{
		for (int i = 0; i < (width - l); i++ )
		{
			switch(style)
			{
			case NUM_FONT_SMALL:
				CG_DrawPic( x,y, charWidth, charHeight, cgs.media.smallnumberShaders[0] );
				break;
			case NUM_FONT_CHUNKY:
				CG_DrawPic( x,y, charWidth, charHeight, cgs.media.chunkyNumberShaders[0] );
				break;
			default:
			case NUM_FONT_BIG:
				CG_DrawPic( x,y, charWidth, charHeight, cgs.media.numberShaders[0] );
				break;
			}
			x += 2 + (xWidth);
		}
	}
	else
	{
		x += 2 + (xWidth)*(width - l);
	}

	ptr = num;
	while (*ptr && l)
	{
		if (*ptr == '-')
			frame = STAT_MINUS;
		else
			frame = *ptr -'0';

		switch(style)
		{
		case NUM_FONT_SMALL:
			CG_DrawPic( x,y, charWidth, charHeight, cgs.media.smallnumberShaders[frame] );
			x++;	// For a one line gap
			break;
		case NUM_FONT_CHUNKY:
			CG_DrawPic( x,y, charWidth, charHeight, cgs.media.chunkyNumberShaders[frame] );
			break;
		default:
		case NUM_FONT_BIG:
			CG_DrawPic( x,y, charWidth, charHeight, cgs.media.numberShaders[frame] );
			break;
		}

		x += (xWidth);
		ptr++;
		l--;
	}

}

/*
=================
CG_DrawProportionalString
=================
*/
void CG_DrawProportionalString( int x, int y, const char* str, int style, vec4_t color, float aspectCorrection)
{
	//assert(!style);//call this directly if you need style (OR it into the font handle)
	cgi_R_Font_DrawString (x, y, str, color, cgs.media.qhFontMedium, -1, 1.0f, aspectCorrection);
}
#pragma region Text Fitting

#define FIT_MAX_LINES		8
#define FIT_LINE_CHARS		256

// Returns the space closest to the middle of psLine, or NULL if it holds none.
static const char* CG_MiddleSpace(const char* psLine)
{
	int len = (int)strlen(psLine);
	int mid = len / 2;
	const char* best = NULL;
	int bestDist = len;

	for (int i = 0; i < len; i++)
	{
		if (psLine[i] != ' ')
			continue;

		int dist = (i > mid) ? (i - mid) : (mid - i);
		if (dist < bestDist)
		{
			bestDist = dist;
			best = psLine + i;
		}
	}
	return best;
}

// Widest of the current lines, in pixels.
static int CG_WidestLine(char lines[FIT_MAX_LINES][FIT_LINE_CHARS], int numLines,
	int iFontHandle, float fScale)
{
	int widest = 0;

	for (int i = 0; i < numLines; i++)
	{
		int w = cgi_R_Font_StrLenPixels(lines[i], iFontHandle, fScale, cgs.widthRatioCoef);
		if (w > widest)
			widest = w;
	}
	return widest;
}

// Cuts the widest line on its most central space, so the caller gains exactly one line.
// Returns qfalse when that line holds no space to cut on. The caller owns numLines and
// must leave a free slot, so call it only while numLines < FIT_MAX_LINES.
static qboolean CG_SplitWidestLine(char lines[FIT_MAX_LINES][FIT_LINE_CHARS], int numLines,
	int iFontHandle, float fScale)
{
	int widest = 0, widestWidth = -1;

	for (int i = 0; i < numLines; i++)
	{
		int w = cgi_R_Font_StrLenPixels(lines[i], iFontHandle, fScale, cgs.widthRatioCoef);
		if (w > widestWidth)
		{
			widestWidth = w;
			widest = i;
		}
	}

	const char* space = CG_MiddleSpace(lines[widest]);
	if (!space)
		return qfalse;

	// push the tail down a slot, then cut - the tail is read before the line is truncated
	for (int i = numLines; i > widest + 1; i--)
		Q_strncpyz(lines[i], lines[i - 1], FIT_LINE_CHARS);

	Q_strncpyz(lines[widest + 1], space + 1, FIT_LINE_CHARS);
	lines[widest][(int)(space - lines[widest])] = '\0';

	return qtrue;
}

// Draws psText in the box, horizontally aligned per iAlign. The scale starts at one
// line filling the box height;
// while the text is too wide, the widest line is cut on its most central space and the
// scale becomes base/numLines - so the block always stays exactly one box high.
void CG_DrawTextInBox(int iBoxX, int iBoxY, int iBoxWidth, int iBoxHeight,
	const char* psText, int iFontHandle, const vec4_t v4Color, textBoxAlign_t iAlign)
{
	char	lines[FIT_MAX_LINES][FIT_LINE_CHARS];
	int		numLines = 1;
	int		iFontHeight = cgi_R_Font_HeightPixels(iFontHandle, 1.0f);

	if (iFontHeight <= 0)
		return;

	float originalScale = (float)iBoxHeight / (float)iFontHeight;
	float fScale = originalScale;

	Q_strncpyz(lines[0], psText, FIT_LINE_CHARS);

	// Too wide: make the font smaller twice (x0.85), then split the widest line and start again.
	const float shrinkFactor = 0.85f;
	const int maxShrinks = 2;
	int shrinks = 0;
	while (CG_WidestLine(lines, numLines, iFontHandle, fScale) > iBoxWidth)
	{
		if (shrinks < maxShrinks)
		{
			fScale *= shrinkFactor;
			shrinks++;
			continue;
		}
		if (numLines >= FIT_MAX_LINES || !CG_SplitWidestLine(lines, numLines, iFontHandle, fScale))
		{
			break;
		}
		numLines++;
		fScale = originalScale / (float)numLines;
		shrinks = 0;
	}

	int iLineHeight = cgi_R_Font_HeightPixels(iFontHandle, fScale);
	int y = iBoxY + ((iBoxHeight - (numLines * iLineHeight)) / 2);

	for (int i = 0; i < numLines; i++)
	{
		int w = cgi_R_Font_StrLenPixels(lines[i], iFontHandle, fScale, cgs.widthRatioCoef);
		int x;

		switch (iAlign)
		{
			case ALIGN_LEFT:	
				x = iBoxX;		
				break;
			case ALIGN_RIGHT:
				x = iBoxX + iBoxWidth - w;		
				break;
			default:			
				x = iBoxX + ((iBoxWidth - w) / 2);	
				break;
		}

		cgi_R_Font_DrawString(x, y + (i * iLineHeight) + (numLines == 1 ? 0 : numLines), //Adding numline to Y to "correct" the font padding that is divided by two
			lines[i], v4Color, iFontHandle, -1, fScale, cgs.widthRatioCoef);	// no clip: the lines fit already, and the clip of the renderer can drop the last letter by less than a pixel
	}
}
#pragma endregion
