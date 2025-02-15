/*$
 Copyright (C) 2013-2025 Azel.

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

/**************************************
 * フィルタ処理: アンチエイリアシング
 **************************************/

#include <mlk.h>

#include "def_filterdraw.h"

#include "tileimage.h"
#include "tileimage_drawinfo.h"

#include "pv_filter_sub.h"


/*
 * TANE 氏のソースより。
 * http://www5.ocn.ne.jp/~tane/
 *
 * layman-0.62.tar.gz : lervise.c
 */


/* 色の比較 */

static mlkbool _comp_color(FilterDrawInfo *info,void *col1,void *col2)
{
	if(info->bits == 8)
	{
		RGBA8 *p8a,*p8b;

		p8a = (RGBA8 *)col1;
		p8b = (RGBA8 *)col2;
		
		return ((p8a->a == 0 && p8b->a == 0)
			|| (p8a->a && p8b->a
				&& (p8a->r >> 2) == (p8b->r >> 2)
				&& (p8a->g >> 2) == (p8b->g >> 2)
				&& (p8a->b >> 2) == (p8b->b >> 2)));
	}
	else
	{
		RGBA16 *p16a,*p16b;

		p16a = (RGBA16 *)col1;
		p16b = (RGBA16 *)col2;

		return ((p16a->a == 0 && p16b->a == 0)
			|| (p16a->a && p16b->a
				&& (p16a->r >> 9) == (p16b->r >> 9)
				&& (p16a->g >> 9) == (p16b->g >> 9)
				&& (p16a->b >> 9) == (p16b->b >> 9)));
	}
}

/* 色の比較 (位置) */

static mlkbool _comp_color_at(FilterDrawInfo *info,TileImage *img,int x1,int y1,int x2,int y2)
{
	uint64_t col1,col2;

	TileImage_getPixel(img, x1, y1, &col1);
	TileImage_getPixel(img, x2, y2, &col2);

	return _comp_color(info, &col1, &col2);
}

/* 点の描画 */

static void _setpixel(FilterDrawInfo *info,int sx,int sy,int dx,int dy,int opacity)
{
	int i;
	double sa,da,na;
	uint64_t col,col1,col2;

	if(opacity <= 0) return;

	TileImage_getPixel(info->imgsrc, sx, sy, &col1);
	TileImage_getPixel(info->imgsrc, dx, dy, &col2);

	if(_comp_color(info, &col1, &col2)) return;

	//合成

	if(info->bits == 8)
	{
		//8bit
		
		uint8_t *p8a,*p8b,*p8d;

		p8a = (uint8_t *)&col1;
		p8b = (uint8_t *)&col2;
		p8d = (uint8_t *)&col;
		
		sa = (double)(p8a[3] * opacity / 255) / 255;
		da = (double)p8b[3] / 255;
		na = sa + da - sa * da;

		p8d[3] = (int)(na * 255 + 0.5);

		if(p8d[3] == 0)
			col = 0;
		else
		{
			da = da * (1 - sa);
			na = 1.0 / na;

			for(i = 0; i < 3; i++)
				p8d[i] = (uint8_t)((p8a[i] * sa + p8b[i] * da) * na + 0.5);
		}
	}
	else
	{
		//16bit

		uint16_t *p16a,*p16b,*p16d;

		p16a = (uint16_t *)&col1;
		p16b = (uint16_t *)&col2;
		p16d = (uint16_t *)&col;
		
		sa = (double)(p16a[3] * opacity >> 15) / 0x8000;
		da = (double)p16b[3] / 0x8000;
		na = sa + da - sa * da;

		p16d[3] = (int)(na * 0x8000 + 0.5);

		if(p16d[3] == 0)
			col = 0;
		else
		{
			da = da * (1 - sa);
			na = 1.0 / na;

			for(i = 0; i < 3; i++)
				p16d[i] = (int)((p16a[i] * sa + p16b[i] * da) * na + 0.5);
		}
	}

	(g_tileimage_dinfo.func_setpixel)(info->imgdst, dx, dy, &col);
}

/* 描画 */

static void _aa_draw(FilterDrawInfo *info,int x,int y,mlkbool right,mRect *rc)
{
	int x1,y1,x2,y2,i,a,cnt,csub;

	csub = (info->bits == 8)? 16: 1966;

	//上

	x1 = x2 = x;
	if(right) x1++; else x2++;

	if(rc->y1 == 0)
		_setpixel(info, x1, y, x2, y, info->ntmp[0] >> 1);
	else
	{
		cnt = (rc->y1 + 1) >> 1;
		if(y - cnt + 1 < info->rc.y1) cnt = y - info->rc.y1 + 1;

		a = info->ntmp[0];
		if(cnt < 4) a -= csub;

		for(i = 0; i <= cnt; i++)
			_setpixel(info, x1, y, x2, y - i, a - (a * i / cnt));
	}

	//下

	x1 = x2 = x;
	y1 = y2 = y + 1;
	if(right) x2++; else x1++;

	if(rc->y2 == 0)
		_setpixel(info, x1, y1, x2, y2, info->ntmp[0] >> 1);
	else
	{
		cnt = (rc->y2 + 1) >> 1;
		if(y + cnt > info->rc.y2) cnt = info->rc.y2 - y;

		a = info->ntmp[0];
		if(cnt < 4) a -= csub;

		for(i = 0; i <= cnt; i++)
			_setpixel(info, x1, y1, x2, y2 + i, a - (a * i / cnt));
	}

	//左

	y1 = y2 = y;
	if(right) y1++; else y2++;

	if(rc->x1)
	{
		cnt = (rc->x1 + 1) >> 1;
		if(x - cnt + 1 < info->rc.x1) cnt = x - info->rc.x1 + 1;

		a = info->ntmp[0];
		if(cnt < 4) a -= csub;

		for(i = 0; i <= cnt; i++)
			_setpixel(info, x, y1, x - i, y2, a - (a * i / cnt));
	}

	//右

	x1 = x2 = x + 1;
	y1 = y2 = y;
	if(right) y2++; else y1++;

	if(rc->x2)
	{
		cnt = (rc->x2 + 1) >> 1;
		if(x + cnt > info->rc.x2) cnt = info->rc.x2 - x;

		a = info->ntmp[0];
		if(cnt < 4) a -= csub;

		for(i = 0; i <= cnt; i++)
			_setpixel(info, x1, y1, x2 + i, y2, a - (a * i / cnt));
	}
}

/* 判定 */

static void _aa_func(FilterDrawInfo *info,int x,int y,mlkbool right)
{
	TileImage *imgsrc = info->imgsrc;
	mRect rc;
	int i,x1,x2,y1,y2;
	uint64_t col,col2,col3;

	rc.x1 = rc.y1 = rc.x2 = rc.y2 = 0;

	//上に伸びているか

	x1 = x2 = x;
	if(right) x1++; else x2++;

	if(!_comp_color_at(info, imgsrc, x1, y, x2, y))
	{
		TileImage_getPixel(imgsrc, x1, y, &col);

		for(i = y - 1; i >= info->rc.y1; i--)
		{
			TileImage_getPixel(imgsrc, x1, i, &col2);
			TileImage_getPixel(imgsrc, x2, i, &col3);

			if(_comp_color(info, &col, &col2) && !_comp_color(info, &col, &col3))
				rc.y1++;
			else
				break;
		}
	}

	//下に伸びているか

	x1 = x2 = x;
	y1 = y + 1;
	if(right) x2++; else x1++;

	if(!_comp_color_at(info, imgsrc, x1, y1, x2, y1))
	{
		TileImage_getPixel(imgsrc, x1, y1, &col);

		for(i = y + 2; i <= info->rc.y2; i++)
		{
			TileImage_getPixel(imgsrc, x1, i, &col2);
			TileImage_getPixel(imgsrc, x2, i, &col3);

			if(_comp_color(info, &col, &col2) && !_comp_color(info, &col, &col3))
				rc.y2++;
			else
				break;
		}
	}

	//左に伸びているか

	y1 = y2 = y;
	if(right) y1++; else y2++;

	if(!_comp_color_at(info, imgsrc, x, y1, x, y2))
	{
		TileImage_getPixel(imgsrc, x, y1, &col);

		for(i = x - 1; i >= info->rc.x1; i--)
		{
			TileImage_getPixel(imgsrc, i, y1, &col2);
			TileImage_getPixel(imgsrc, i, y2, &col3);

			if(_comp_color(info, &col, &col2) && !_comp_color(info, &col, &col3))
				rc.x1++;
			else
				break;
		}
	}

	//右に伸びているか

	x1 = x + 1;
	y1 = y2 = y;
	if(right) y2++; else y1++;

	if(!_comp_color_at(info, imgsrc, x1, y1, x1, y2))
	{
		TileImage_getPixel(imgsrc, x1, y1, &col);

		for(i = x + 2; i <= info->rc.x2; i++)
		{
			TileImage_getPixel(imgsrc, i, y1, &col2);
			TileImage_getPixel(imgsrc, i, y2, &col3);

			if(_comp_color(info, &col, &col2) && !_comp_color(info, &col, &col3))
				rc.x2++;
			else
				break;
		}
	}

	_aa_draw(info, x, y, right, &rc);
}

/* アンチエイリアシング */

mlkbool FilterDraw_antialiasing(FilterDrawInfo *info)
{
	TileImage *imgsrc;
	int ix,iy,f;
	uint64_t col,colx,coly,colxy;

	if(info->bits == 8)
		info->ntmp[0] = (info->val_bar[0] << 7) / 100;
	else
		info->ntmp[0] = (info->val_bar[0] << 14) / 100;

	FilterSub_copySrcImage_forPreview(info);

	imgsrc = info->imgsrc;

	//[!] 右端、下端は処理しない

	FilterSub_prog_substep_begin_onestep(info, 50, info->box.h - 1);

	for(iy = info->rc.y1; iy < info->rc.y2; iy++)
	{
		for(ix = info->rc.x1; ix < info->rc.x2; ix++)
		{
			TileImage_getPixel(imgsrc, ix, iy, &col);
			TileImage_getPixel(imgsrc, ix + 1, iy, &colx);
			TileImage_getPixel(imgsrc, ix, iy + 1, &coly);
			TileImage_getPixel(imgsrc, ix + 1, iy + 1, &colxy);

			f = _comp_color(info, &col, &colxy)
				| (_comp_color(info, &colx, &coly) << 1)
				| (_comp_color(info, &col, &coly) << 2);

			if(f == 7) continue;

			if(f & 1)
				_aa_func(info, ix, iy, FALSE);

			if(f & 2)
				_aa_func(info, ix, iy, TRUE);
		}

		FilterSub_prog_substep_inc(info);
	}

	return TRUE;
}

