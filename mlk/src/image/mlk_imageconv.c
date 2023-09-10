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

/*****************************************
 * イメージ変換
 *****************************************/

#include <string.h>

#include <mlk.h>
#include <mlk_imageconv.h>


/**@ RGB/RGBA(8bit) の R と B を入れ替える
 *
 * @d:RGB <-> BGR 変換を行う。
 * @p:bytes 1px のバイト数 (3 or 4) */

void mImageConv_swap_rb_8(uint8_t *buf,uint32_t width,int bytes)
{
	uint8_t a,b;

	for(; width; width--, buf += bytes)
	{
		a = buf[0], b = buf[2];
		buf[0] = b, buf[2] = a;
	}
}

/**@ RGB(8bit) -> RGBA(8bit) へ拡張
 *
 * @d:同じバッファ上でアルファ値を追加して、拡張する。\
 * アルファ値は 255 となる。 */

void mImageConv_rgb8_to_rgba8_extend(uint8_t *buf,uint32_t width)
{
	uint8_t *ps,*pd,r,g,b;

	ps = buf + (width - 1) * 3;
	pd = buf + ((width - 1) << 2);

	for(; width; width--, ps -= 3, pd -= 4)
	{
		r = ps[0], g = ps[1], b = ps[2];

		pd[0] = r;
		pd[1] = g;
		pd[2] = b;
		pd[3] = 255;
	}
}


//====================================
// RGBX からの変換
//====================================


/**@ RGBX(8bit) -> RGB(8bit) */

void mImageConv_rgbx8_to_rgb8(uint8_t *dst,const uint8_t *src,uint32_t width)
{
	for(; width; width--, dst += 3, src += 4)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
	}
}

/**@ RGBX(8bit) -> GRAY(8bit)
 *
 * @d:グレイスケール化。R の値を使う。 */

void mImageConv_rgbx8_to_gray8(uint8_t *dst,const uint8_t *src,uint32_t width)
{
	for(; width; width--, src += 4)
		*(dst++) = *src;
}

/**@ RGBX(8bit) -> GRAY(1bit)
 *
 * @d:白黒化。\
 * ビットは上位から順にセット。\
 * R の最上位ビットが ON かどうかで判定する。 */

void mImageConv_rgbx8_to_gray1(uint8_t *dst,const uint8_t *src,uint32_t width)
{
	uint8_t f = 0x80,val = 0;

	for(; width; width--, src += 4)
	{
		if(*src & 0x80)
			val |= f;

		f >>= 1;
		if(!f)
		{
			*(dst++) = val;
			f = 0x80, val = 0;
		}
	}

	//残り

	if(f != 0x80)
		*dst = val;
}


//====================================
// RGB などへの変換 (主に読み込み用)
//====================================


/**@ Gray (8bit) -> Gray/RGB/RGBA (8bit)
 *
 * @d:値反転有効。 */

void mImageConv_gray8(mImageConv *p)
{
	const uint8_t *ps;
	uint8_t *pd,c;
	int type,frev;
	uint32_t i;

	ps = (const uint8_t *)p->srcbuf;
	pd = (uint8_t *)p->dstbuf;

	type = p->convtype;
	frev = p->flags & MIMAGECONV_FLAGS_REVERSE;

	//コピー

	if(type == MIMAGECONV_CONVTYPE_NONE && !frev)
	{
		memcpy(pd, ps, p->width);
		return;
	}

	//

	for(i = p->width; i; i--)
	{
		c = *(ps++);
		if(frev) c = 255 - c;

		if(type == MIMAGECONV_CONVTYPE_NONE)
			//Gray
			*(pd++) = c;
		else
		{
			//RGB/RGBA
			
			pd[0] = pd[1] = pd[2] = c;

			if(type == MIMAGECONV_CONVTYPE_RGBA)
				pd[3] = 255, pd += 4;
			else
				pd += 3;
		}
	}
}

/**@ Gray (1/2/4/8 bit) -> Gray/RGB/RGBA (8bit) */

void mImageConv_gray_1_2_4_8(mImageConv *p)
{
	const uint8_t *ps;
	uint8_t *pd;
	int bits,shift,mask,type,c,mul,reverse;
	uint32_t i;

	ps = (const uint8_t *)p->srcbuf;
	pd = (uint8_t *)p->dstbuf;

	type = p->convtype;
	bits = p->srcbits;
	shift = 8 - bits;
	mask = (1 << bits) - 1;
	mul = (bits == 1)? 255: 255 / ((1 << bits) - 1);
	reverse = ((p->flags & MIMAGECONV_FLAGS_REVERSE) != 0);

	//コピー

	if(type == MIMAGECONV_CONVTYPE_NONE
		&& bits == 8 && !reverse)
	{
		memcpy(pd, ps, p->width);
		return;
	}

	//

	for(i = p->width; i; i--)
	{
		c = (*ps >> shift) & mask;

		if(reverse) c = mask - c;

		c *= mul;

		//

		if(type == MIMAGECONV_CONVTYPE_NONE)
			//8bit
			*(pd++) = c;
		else
		{
			//RGB/RGBA
			
			pd[0] = pd[1] = pd[2] = c;

			if(type == MIMAGECONV_CONVTYPE_RGBA)
				pd[3] = 255, pd += 4;
			else
				pd += 3;
		}

		//
	
		shift -= bits;
		if(shift < 0)
		{
			shift = 8 - bits;
			ps++;
		}
	}
}

/**@ Gray (16bit) -> GRAY/RGB/RGBA (8/16bit)
 *
 * @d:ソースはホストのバイト順。 */

void mImageConv_gray16(mImageConv *p)
{
	const uint16_t *ps;
	uint8_t *pd8;
	uint16_t *pd16,c;
	int type,to8bit,reverse;
	uint32_t i;

	ps   = (const uint16_t *)p->srcbuf;
	pd8  = (uint8_t *)p->dstbuf;
	pd16 = (uint16_t *)pd8;

	type = p->convtype;
	to8bit = (p->dstbits == 8);
	reverse = ((p->flags & MIMAGECONV_FLAGS_REVERSE) != 0);

	//コピー

	if(type == MIMAGECONV_CONVTYPE_NONE
		&& !to8bit && !reverse)
	{
		memcpy(pd8, ps, p->width * 2);
		return;
	}

	//

	for(i = p->width; i; i--)
	{
		c = *(ps++);

		if(reverse) c = 0xffff - c;

		//セット

		if(to8bit)
		{
			//8bit

			c >>= 8;

			if(type == MIMAGECONV_CONVTYPE_NONE)
				*(pd8++) = c;
			else
			{
				pd8[0] = pd8[1] = pd8[2] = c;

				if(type == MIMAGECONV_CONVTYPE_RGBA)
					pd8[3] = 255, pd8 += 4;
				else
					pd8 += 3;
			}
		}
		else
		{
			//16bit
			
			if(type == MIMAGECONV_CONVTYPE_NONE)
				*(pd16++) = c;
			else
			{
				pd16[0] = pd16[1] = pd16[2] = c;

				if(type == MIMAGECONV_CONVTYPE_RGBA)
					pd16[3] = 0xffff, pd16 += 4;
				else
					pd16 += 3;
			}
		}
	}
}

/**@ PAL (1/2/4/8bit) -> PAL/RGB/RGBA (8bit)
 *
 * @d:RGBA 変換の場合、パレットのアルファ値をセットする。 */

void mImageConv_palette_1_2_4_8(mImageConv *p)
{
	const uint8_t *ps,*ppal;
	uint8_t *pd;
	int bits,shift,mask,type,c;
	uint32_t i;

	ps = (const uint8_t *)p->srcbuf;
	pd = (uint8_t *)p->dstbuf;

	type = p->convtype;
	bits = p->srcbits;
	shift = 8 - bits;
	mask = (1 << bits) - 1;

	//コピー

	if(type == MIMAGECONV_CONVTYPE_NONE && bits == 8)
	{
		memcpy(pd, ps, p->width);
		return;
	}

	//

	for(i = p->width; i; i--)
	{
		c = (*ps >> shift) & mask;

		if(type == MIMAGECONV_CONVTYPE_NONE)
			//8bit
			*(pd++) = c;
		else
		{
			//RGB/RGBA
			
			ppal = p->palbuf + (c << 2);

			pd[0] = ppal[0];
			pd[1] = ppal[1];
			pd[2] = ppal[2];

			if(type == MIMAGECONV_CONVTYPE_RGBA)
				pd[3] = ppal[3], pd += 4;
			else
				pd += 3;
		}

		//
	
		shift -= bits;
		if(shift < 0)
		{
			shift = 8 - bits;
			ps++;
		}
	}
}

/**@ RGB (16bit:各5bit) -> RGB/RGBA (8bit)
 *
 * @d:ソースはホストエンディアン。\
 * SRC_ALPHA が ON の場合、最上位ビットをアルファ値として扱う。 */

void mImageConv_rgb555(mImageConv *p)
{
	const uint16_t *ps;
	uint8_t *pd;
	uint32_t i,c;
	int r,g,b,a,src_alpha,dst_alpha;

	ps = (const uint16_t *)p->srcbuf;
	pd = (uint8_t *)p->dstbuf;

	src_alpha = ((p->flags & MIMAGECONV_FLAGS_SRC_ALPHA) != 0);

	dst_alpha = (p->convtype == MIMAGECONV_CONVTYPE_RGBA
		|| (p->convtype == MIMAGECONV_CONVTYPE_NONE && src_alpha));

	a = 255;

	for(i = p->width; i; i--)
	{
		c = *(ps++);

		r = ((c >> 10) & 31) << 3;
		g = ((c >> 5) & 31) << 3;
		b = (c & 31) << 3;

		if(r > 255) r = 255;
		if(g > 255) g = 255;
		if(b > 255) b = 255;

		if(src_alpha)
			a = (c & 0x8000)? 255: 0;

		//

		pd[0] = r;
		pd[1] = g;
		pd[2] = b;

		if(dst_alpha)
			pd[3] = a, pd += 4;
		else
			pd += 3;
	}
}

/**@ RGB/BGR (8bit) -> RGB/RGBA (8bit)
 *
 * @d:BGR 順なら入れ替え。\
 * RGBA 変換の場合、A = 255 になる。 */

void mImageConv_rgb8(mImageConv *p)
{
	const uint8_t *ps;
	uint8_t *pd;
	int dst_alpha,is_bgr;
	uint32_t i;

	ps = (const uint8_t *)p->srcbuf;
	pd = (uint8_t *)p->dstbuf;

	is_bgr = ((p->flags & MIMAGECONV_FLAGS_SRC_BGRA) != 0);
	dst_alpha = (p->convtype == MIMAGECONV_CONVTYPE_RGBA);

	//RGB -> RGB

	if(!dst_alpha && !is_bgr)
	{
		memcpy(pd, ps, p->width * 3);
		return;
	}

	//RGB/BGR -> RGB/RGBA

	for(i = p->width; i; i--)
	{
		if(is_bgr)
		{
			pd[0] = ps[2];
			pd[1] = ps[1];
			pd[2] = ps[0];
		}
		else
		{
			pd[0] = ps[0];
			pd[1] = ps[1];
			pd[2] = ps[2];
		}

		if(dst_alpha)
			pd[3] = 255, pd += 4;
		else
			pd += 3;
		
		ps += 3;
	}
}

/**@ RGBA/BGRA (8bit) -> RGB/RGBA (8bit)
 *
 * @d:B-G-R 順なら入れ替え。INVALID_ALPHA 有効。\
 *  変換が NONE (RGBA) でアルファ値が無効の場合は、RGB として出力する。 */

void mImageConv_rgba8(mImageConv *p)
{
	const uint8_t *ps;
	uint8_t *pd;
	int dst_alpha,is_bgr,invalid_alpha;
	uint32_t i;

	ps = (const uint8_t *)p->srcbuf;
	pd = (uint8_t *)p->dstbuf;

	is_bgr = ((p->flags & MIMAGECONV_FLAGS_SRC_BGRA) != 0);
	invalid_alpha = ((p->flags & MIMAGECONV_FLAGS_INVALID_ALPHA) != 0);

	//出力が RGBA か
	// :NONE でアルファ値無効なら、RGB 出力

	dst_alpha = (p->convtype == MIMAGECONV_CONVTYPE_RGBA
		|| (p->convtype == MIMAGECONV_CONVTYPE_NONE && !invalid_alpha));

	//RGBA -> RGBA (アルファ値無効でない時)

	if(dst_alpha && !is_bgr && !invalid_alpha)
	{
		memcpy(pd, ps, p->width * 4);
		return;
	}

	//RGBA(BGRA) -> RGB/RGBA

	for(i = p->width; i; i--)
	{
		if(is_bgr)
		{
			pd[0] = ps[2];
			pd[1] = ps[1];
			pd[2] = ps[0];
		}
		else
		{
			pd[0] = ps[0];
			pd[1] = ps[1];
			pd[2] = ps[2];
		}

		if(dst_alpha)
		{
			pd[3] = (invalid_alpha)? 255: ps[3];
			pd += 4;
		}
		else
			pd += 3;
		
		ps += 4;
	}
}

/**@ RGB/RGBA (16bit) -> RGB/RGBA (8/16bit)
 *
 * @d:16bit から、8/16bit への変換。\
 * ソースが RGBA かどうかは、SRC_ALPHA で判定。\
 * ソースはホストエンディアン。 */

void mImageConv_rgb_rgba_16(mImageConv *p)
{
	const uint16_t *ps;
	uint8_t *pd8;
	uint16_t *pd16,r,g,b,a;
	uint32_t i;
	uint8_t dst_alpha,to8bit,src_alpha;

	ps   = (const uint16_t *)p->srcbuf;
	pd8  = (uint8_t *)p->dstbuf;
	pd16 = (uint16_t *)pd8;

	dst_alpha = (p->convtype == MIMAGECONV_CONVTYPE_RGBA);
	src_alpha = ((p->flags & MIMAGECONV_FLAGS_SRC_ALPHA) != 0);
	to8bit = (p->dstbits == 8);

	//

	for(i = p->width; i; i--)
	{
		//16bit src

		r = ps[0];
		g = ps[1];
		b = ps[2];

		if(src_alpha)
			a = ps[3], ps += 4;
		else
			a = 0xffff, ps += 3;
		
		//セット

		if(to8bit)
		{
			//8bit
			
			pd8[0] = r >> 8;
			pd8[1] = g >> 8;
			pd8[2] = b >> 8;

			if(dst_alpha)
				pd8[3] = a >> 8, pd8 += 4;
			else
				pd8 += 3;
		}
		else
		{
			//16bit
			
			pd16[0] = r;
			pd16[1] = g;
			pd16[2] = b;

			if(dst_alpha)
				pd16[3] = a, pd16 += 4;
			else
				pd16 += 3;
		}
	}
}

/**@ CMYK (8bit) -> CMYK/RGB/RGBA (8bit)
 *
 * @d:CMYK にはアルファ値はない。値の反転可。 */

void mImageConv_cmyk8(mImageConv *p)
{
	uint8_t *pd;
	const uint8_t *ps;
	int frev,falpha,c,m,y,k;
	uint32_t i;
	double d;

	ps = (const uint8_t *)p->srcbuf;
	pd = (uint8_t *)p->dstbuf;

	frev = (p->flags & MIMAGECONV_FLAGS_REVERSE);

	//CMYK のまま
	
	if(p->convtype == MIMAGECONV_CONVTYPE_NONE)
	{
		if(frev)
		{
			//反転
			for(i = p->width * 4; i; i--, ps++, pd++)
				*pd = 255 - *ps;
		}
		else
			memcpy(pd, ps, p->width * 4);

		return;
	}

	//CMYK -> RGB/RGBA 変換

	falpha = (p->convtype == MIMAGECONV_CONVTYPE_RGBA);

	for(i = p->width; i; i--)
	{
		c = ps[0];
		m = ps[1];
		y = ps[2];
		k = ps[3];
		ps += 4;

		if(frev)
		{
			c = 255 - c;
			m = 255 - m;
			y = 255 - y;
			k = 255 - k;
		}

		//RGB
	
		d = (255 - k) / 255.0;
		
		pd[0] = 255 - (uint8_t)(c * d + k + 0.5);
		pd[1] = 255 - (uint8_t)(m * d + k + 0.5);
		pd[2] = 255 - (uint8_t)(y * d + k + 0.5);

		if(falpha)
		{
			pd[3] = 255;
			pd += 4;
		}
		else
			pd += 3;
	}
}

/**@ CMYK (16bit) -> CMYK/RGB/RGBA (8/16bit)
 *
 * @d:CMYK にはアルファ値はない。値の反転可。\
 * ソースはホストエンディアン。 */

void mImageConv_cmyk16(mImageConv *p)
{
	const uint16_t *ps;
	uint16_t *pd16;
	uint8_t *pd8;
	int frev,falpha,fcmyk,to8bit,c,m,y,k;
	uint32_t i;
	double d;

	ps = (const uint16_t *)p->srcbuf;
	pd16 = (uint16_t *)p->dstbuf;
	pd8 = (uint8_t *)pd16;

	frev = (p->flags & MIMAGECONV_FLAGS_REVERSE);
	fcmyk = (p->convtype == MIMAGECONV_CONVTYPE_NONE);
	falpha = (p->convtype == MIMAGECONV_CONVTYPE_RGBA);
	to8bit = (p->dstbits == 8);

	for(i = p->width; i; i--)
	{
		//CMYK 16bit
		
		c = ps[0];
		m = ps[1];
		y = ps[2];
		k = ps[3];
		ps += 4;

		if(frev)
		{
			c = 0xffff - c;
			m = 0xffff - m;
			y = 0xffff - y;
			k = 0xffff - k;
		}

		//dst

		if(fcmyk)
		{
			//CMYK

			if(to8bit)
			{
				//8bit

				pd8[0] = c >> 8;
				pd8[1] = m >> 8;
				pd8[2] = y >> 8;
				pd8[3] = k >> 8;
				pd8 += 4;
			}
			else
			{
				pd16[0] = c;
				pd16[1] = m;
				pd16[2] = y;
				pd16[3] = k;
				pd16 += 4;
			}
		}
		else
		{
			//RGB
			
			d = (double)(0xffff - k) / 0xffff;
			
			c = 0xffff - (uint16_t)(c * d + k + 0.5);
			m = 0xffff - (uint16_t)(m * d + k + 0.5);
			y = 0xffff - (uint16_t)(y * d + k + 0.5);

			if(to8bit)
			{
				//8bit

				pd8[0] = c >> 8;
				pd8[1] = m >> 8;
				pd8[2] = y >> 8;

				if(falpha)
					pd8[3] = 255, pd8 += 4;
				else
					pd8 += 3;
			}
			else
			{
				//16bit

				pd16[0] = c;
				pd16[1] = m;
				pd16[2] = y;

				if(falpha)
					pd16[3] = 0xffff, pd16 += 4;
				else
					pd16 += 3;
			}
		}
	}
}


//===============================
// チャンネル分離のデータから
//===============================


/**@ [チャンネル] GRAY+A (8bit) -> GRAY+A/RGB/RGBA
 *
 * @g:チャンネルデータ
 *
 * @d:PSD など、チャンネルごとにデータが分かれている場合に使う。\
 * 各チャンネルを結合した位置に出力。\
 * chno: [0] gray [1] alpha */

void mImageConv_sepch_gray_a_8(mImageConv *p)
{
	uint8_t *pd,c;
	const uint8_t *ps;
	uint32_t i;

	pd = (uint8_t *)p->dstbuf;
	ps = (const uint8_t *)p->srcbuf;
	i = p->width;

	//

	if(p->convtype == MIMAGECONV_CONVTYPE_NONE)
	{
		//GRAY+A

		pd += p->chno;

		for(; i; i--, pd += 2)
			*pd = *(ps++);
	}
	else if(p->convtype == MIMAGECONV_CONVTYPE_RGB)
	{
		//RGB

		if(p->chno != 0) return;

		for(; i; i--, pd += 3)
		{
			c = *(ps++);
			pd[0] = pd[1] = pd[2] = c;
		}
	}
	else
	{
		//RGBA

		if(p->chno == 0)
		{
			for(; i; i--, pd += 4)
			{
				c = *(ps++);
				pd[0] = pd[1] = pd[2] = c;
			}
		}
		else
		{
			//Alpha

			for(pd += 3; i; i--, pd += 4)
				*pd = *(ps++);
		}
	}
}

/**@ [チャンネル] GRAY+A (16bit) -> GRAY+A/RGB/RGBA (8/16bit)
 *
 * @d:chno == 1 のソースがアルファ値となる。\
 * ソースのエンディアンはホスト順。 */

void mImageConv_sepch_gray_a_16(mImageConv *p)
{
	const uint16_t *ps;
	uint8_t *pd8;
	uint16_t *pd16,c;
	uint32_t i;
	int type,chno,add,to8bit;

	//RGB 変換時、アルファ値は必要ない

	if(p->convtype == MIMAGECONV_CONVTYPE_RGB && p->chno == 1)
		return;

	//

	ps = (const uint16_t *)p->srcbuf;
	pd8 = (uint8_t *)p->dstbuf;
	pd16 = (uint16_t *)pd8;

	type = p->convtype;
	chno = p->chno;
	to8bit = (p->dstbits == 8);

	if(type == MIMAGECONV_CONVTYPE_NONE)
		pd16 += chno, pd8 += chno, add = 2;
	else
		add = (type == MIMAGECONV_CONVTYPE_RGB)? 3: 4;

	//

	for(i = p->width; i; i--)
	{
		c = *(ps++);

		if(to8bit)
		{
			//8bit

			c >>= 8;

			if(type == MIMAGECONV_CONVTYPE_NONE)
				*pd8 = c;
			else
			{
				//RGB[A]

				if(chno == 1)
					pd8[3] = c;
				else
					pd8[0] = pd8[1] = pd8[2] = c;
			}

			pd8 += add;
		}
		else
		{
			//16bit

			if(type == MIMAGECONV_CONVTYPE_NONE)
				*pd16 = c;
			else
			{
				//RGB[A]

				if(chno == 1)
					pd16[3] = c;
				else
					pd16[0] = pd16[1] = pd16[2] = c;
			}

			pd16 += add;
		}
	}
}

/**@ [チャンネル] RGB/RGBA (8bit) -> RGB/RGBA (8bit)
 *
 * @d:SRC_ALPHA が ON で、ソースは RGBA。\
 * chno: 0=R, 1=G, 2=B, 3=A。 */

void mImageConv_sepch_rgb_rgba_8(mImageConv *p)
{
	uint8_t *pd;
	const uint8_t *ps;
	uint32_t i;
	int type;

	pd = (uint8_t *)p->dstbuf;
	ps = (const uint8_t *)p->srcbuf;
	i = p->width;
	type = p->convtype;

	//変換なしの場合

	if(type == MIMAGECONV_CONVTYPE_NONE)
	{
		type = (p->flags & MIMAGECONV_FLAGS_SRC_ALPHA)?
			MIMAGECONV_CONVTYPE_RGBA: MIMAGECONV_CONVTYPE_RGB;
	}

	//

	if(type == MIMAGECONV_CONVTYPE_RGB)
	{
		//RGB

		if(p->chno == 3) return;

		for(pd += p->chno; i; i--, pd += 3)
			*pd = *(ps++);
	}
	else
	{
		//RGBA

		for(pd += p->chno; i; i--, pd += 4)
			*pd = *(ps++);

		//元がアルファなしの場合、chno == 0 時にアルファ値をセット

		if(p->chno == 0 && !(p->flags & MIMAGECONV_FLAGS_SRC_ALPHA))
		{
			pd = (uint8_t *)p->dstbuf + 3;
			i = p->width;
			
			for(; i; i--, pd += 4)
				*pd = 255;
		}
	}
}

/**@ [チャンネル] RGB/RGBA (16bit) -> RGB/RGBA (8/16bit)
 *
 * @d:SRC_ALPHA が ON で、ソースは RGBA。\
 * ソースのエンディアンはホスト順。\
 * chno: 0=R, 1=G, 2=B, 3=A。 */

void mImageConv_sepch_rgb_rgba_16(mImageConv *p)
{
	const uint16_t *ps;
	uint8_t *pd8;
	uint16_t *pd16,c;
	uint32_t i;
	int type,to8bit,add;

	type = p->convtype;

	//変換なしの場合

	if(type == MIMAGECONV_CONVTYPE_NONE)
	{
		type = (p->flags & MIMAGECONV_FLAGS_SRC_ALPHA)?
			MIMAGECONV_CONVTYPE_RGBA: MIMAGECONV_CONVTYPE_RGB;
	}

	//RGB 時、アルファ値は必要ない

	if(type == MIMAGECONV_CONVTYPE_RGB && p->chno == 3)
		return;

	//

	ps = (const uint16_t *)p->srcbuf;
	pd8 = (uint8_t *)p->dstbuf + p->chno;
	pd16 = (uint16_t *)p->dstbuf + p->chno;

	to8bit = (p->dstbits == 8);
	add = (type == MIMAGECONV_CONVTYPE_RGB)? 3: 4;

	//

	for(i = p->width; i; i--)
	{
		c = *(ps++);

		if(to8bit)
		{
			//8bit

			*pd8 = c >> 8;
			pd8 += add;
		}
		else
		{
			//16bit

			*pd16 = c;
			pd16 += add;
		}
	}

	//元が RGB で RGBA 変換の場合、chno == 0 時にアルファ値をセット

	if(p->chno == 0
		&& !(p->flags & MIMAGECONV_FLAGS_SRC_ALPHA)
		&& type == MIMAGECONV_CONVTYPE_RGBA)
	{
		i = p->width;

		if(to8bit)
		{
			pd8 = (uint8_t *)p->dstbuf + 3;

			for(; i; i--, pd8 += 4)
				*pd8 = 255;
		}
		else
		{
			pd16 = (uint16_t *)p->dstbuf + 3;

			for(; i; i--, pd16 += 4)
				*pd16 = 0xffff;
		}
	}
}

/**@ [チャンネル] CMYK (8bit) -> CMYK/RGB/RGBA (8bit)
 *
 * @d:chno: 0=C, 1=M, 2=Y, 3=K。\
 * RGB/RGBA 変換の場合、chno = 3 が来た時、CMY の色を元に変換する。\
 * (出力先は最低でも 3byte あるので、CMY をセットしてから、最後に RGB 変換) */

void mImageConv_sepch_cmyk8(mImageConv *p)
{
	const uint8_t *ps;
	uint8_t *pd,c;
	uint32_t i;
	int frev,chno,type,dstbytes;
	double d;

	type = p->convtype;
	chno = p->chno;
	dstbytes = (type == MIMAGECONV_CONVTYPE_RGB)? 3: 4;
	
	pd = (uint8_t *)p->dstbuf;
	ps = (const uint8_t *)p->srcbuf;

	frev = p->flags & MIMAGECONV_FLAGS_REVERSE;

	for(i = p->width; i; i--, pd += dstbytes)
	{
		c = *(ps++);
		if(frev) c = 255 - c;

		if(type == MIMAGECONV_CONVTYPE_NONE || chno < 3)
			//無変換 or CMY をセット
			pd[chno] = c;
		else
		{
			//ch = K の場合、RGB 変換

			d = (255 - c) / 255.0;

			pd[0] = 255 - (int)(pd[0] * d + c + 0.5);
			pd[1] = 255 - (int)(pd[1] * d + c + 0.5);
			pd[2] = 255 - (int)(pd[2] * d + c + 0.5);

			if(type == MIMAGECONV_CONVTYPE_RGBA)
				pd[3] = 255;
		}
	}
}

/**@ [チャンネル] CMYK (16bit) -> CMYK/RGB/RGBA (8/16bit)
 *
 * @d:chno: 0=C, 1=M, 2=Y, 3=K。\
 * ソースのエンディアンはホスト順。 */

void mImageConv_sepch_cmyk16(mImageConv *p)
{
	const uint16_t *ps;
	uint8_t *pd8;
	uint16_t *pd16,c;
	uint32_t i;
	int frev,type,chno,dstbytes,bits;
	double d;

	ps = (const uint16_t *)p->srcbuf;
	pd16 = (uint16_t *)p->dstbuf;
	pd8 = (uint8_t *)pd16;
	
	type = p->convtype;
	chno = p->chno;
	bits = p->dstbits;
	dstbytes = (type == MIMAGECONV_CONVTYPE_RGB)? 3: 4;
	frev = p->flags & MIMAGECONV_FLAGS_REVERSE;

	for(i = p->width; i; i--)
	{
		c = *(ps++);
		if(frev) c = 0xffff - c;

		if(type == MIMAGECONV_CONVTYPE_NONE || chno < 3)
		{
			//無変換 or CMY をセット

			if(bits == 16)
				pd16[chno] = c;
			else
				pd8[chno] = c >> 8;
		}
		else
		{
			//ch = K の場合、RGB 変換

			if(bits == 16)
			{
				d = (double)(0xffff - c) / 0xffff;

				pd16[0] = 0xffff - (int)(pd16[0] * d + c + 0.5);
				pd16[1] = 0xffff - (int)(pd16[1] * d + c + 0.5);
				pd16[2] = 0xffff - (int)(pd16[2] * d + c + 0.5);

				if(type == MIMAGECONV_CONVTYPE_RGBA) pd16[3] = 0xffff;
			}
			else
			{
				//8bit (CMYK は 8bit 変換後の値を使う)
				
				c >>= 8;
				d = (255 - c) / 255.0;
			
				pd8[0] = 255 - (int)(pd8[0] * d + c + 0.5);
				pd8[1] = 255 - (int)(pd8[1] * d + c + 0.5);
				pd8[2] = 255 - (int)(pd8[2] * d + c + 0.5);

				if(type == MIMAGECONV_CONVTYPE_RGBA) pd8[3] = 255;
			}
		}

		pd16 += dstbytes;
		pd8 += dstbytes;
	}
}

