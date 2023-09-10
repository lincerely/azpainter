/*$
 Copyright (C) 2013-2023 Azel.

 This file is part of AzPainter.

 AzPainter is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 AzPainter is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
$*/

/*******************************
 * 塗りつぶし描画処理
 *******************************/

typedef struct _DrawFill DrawFill;

enum
{
	DRAWFILL_TYPE_RGB,
	DRAWFILL_TYPE_TRANSPARENT_AUTO,
	DRAWFILL_TYPE_TRANSPARENT,
	DRAWFILL_TYPE_ALPHA,
	DRAWFILL_TYPE_CANVAS,
	DRAWFILL_TYPE_OPAQUE = 100
};


DrawFill *DrawFill_new(TileImage *imgdst,TileImage *imgref,mPoint *ptstart,
	int type,int color_diff,int draw_density);

void DrawFill_free(DrawFill *p);

void DrawFill_run(DrawFill *p,void *drawcol);

