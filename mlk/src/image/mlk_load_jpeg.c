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
 * JPEG 読み込み
 *****************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //strcmp
#include <setjmp.h>

#include <jpeglib.h>

#include <mlk.h>
#include <mlk_loadimage.h>
#include <mlk_imageconv.h>
#include <mlk_stdio.h>
#include <mlk_util.h>


//--------------------

//JPEG エラー
typedef struct
{
	struct jpeg_error_mgr jerr;
	jmp_buf jmpbuf;
	mLoadImage *loadimg;
	int ferr;
}_myjpeg_err;


typedef struct
{
	struct jpeg_decompress_struct jpg;
	_myjpeg_err jpgerr;

	FILE *fp;
	uint8_t *rowbuf;
	int close_fp;	//終了時に fp を閉じるか
}jpegdata;

//--------------------



//==========================
// エラー関数
//==========================


/* エラー終了時 */

static void _jpeg_error_exit(j_common_ptr ptr)
{
	_myjpeg_err *p = (_myjpeg_err *)ptr->err;
	char msg[JMSG_LENGTH_MAX];

	p->ferr = 1;

	//エラー文字列を取得

	(p->jerr.format_message)(ptr, msg);

	p->loadimg->errmessage = mStrdup(msg);

	//setjmp の位置に飛ぶ
	// :jpeg_finish_decompress() 時に来た場合、
	// :longjmp() が Segmentation fault になるので、回避 

	if(ptr->global_state != 210) //DSTATE_STOPPING
		longjmp(p->jmpbuf, 1);
}

/* メッセージ表示 */

static void _jpeg_output_message(j_common_ptr p)
{

}


//=================================
// 情報読み込み
//=================================


/* EXIF から解像度取得 */

static void _get_exif_resolution(jpegdata *p,mLoadImage *pli)
{
	struct jpeg_marker_struct *plist;

	for(plist = p->jpg.marker_list; plist; plist = plist->next)
	{
		if(plist->marker == 0xe1)
		{
			mLoadImage_getEXIF_resolution(pli,
				(uint8_t *)plist->data, plist->data_length);

			break;
		}
	}
}

/* JFIF の解像度単位を変換 */

static int _get_JFIF_resounit(int unit)
{
	if(unit == 0)
		return MLOADIMAGE_RESOUNIT_ASPECT;
	else if(unit == 1)
		return MLOADIMAGE_RESOUNIT_DPI;
	else if(unit == 2)
		return MLOADIMAGE_RESOUNIT_DPCM;
	else
		return MLOADIMAGE_RESOUNIT_NONE;
}

/* ヘッダ部分読み込み */

static int _read_info(jpegdata *p,mLoadImage *pli)
{
	j_decompress_ptr jpg;
	int colspace,coltype;

	jpg = &p->jpg;

	//エラー設定
	// :デフォルトではエラー時にプロセスが終了するので、longjmp を行うようにする

	jpg->err = jpeg_std_error(&p->jpgerr.jerr);

	p->jpgerr.jerr.error_exit = _jpeg_error_exit;
	p->jpgerr.jerr.output_message = _jpeg_output_message;
	p->jpgerr.loadimg = pli;

	//エラー時

	if(setjmp(p->jpgerr.jmpbuf))
		return MLKERR_LONGJMP;

	//構造体初期化

	jpeg_create_decompress(jpg);

	//------ 各タイプ別の初期化

	switch(pli->open.type)
	{
		//ファイル
		case MLOADIMAGE_OPEN_FILENAME:
			p->fp = mFILEopen(pli->open.filename, "rb");
			if(!p->fp) return MLKERR_OPEN;

			jpeg_stdio_src(jpg, p->fp);
			p->close_fp = TRUE;
			break;
		//FILE *
		case MLOADIMAGE_OPEN_FP:
			jpeg_stdio_src(jpg, (FILE *)pli->open.fp);
			break;
		//バッファ
		case MLOADIMAGE_OPEN_BUF:
			jpeg_mem_src(jpg, (unsigned char *)pli->open.buf, pli->open.size);
			break;
		default:
			return MLKERR_OPEN;
	}

	//-------- 情報

	//保存するマーカー

	jpeg_save_markers(jpg, 0xe1, 0xffff);  //EXIF
	jpeg_save_markers(jpg, JPEG_APP0 + 2, 0xffff); //ICC profile

	//ヘッダ読み込み

	if(jpeg_read_header(jpg, TRUE) != JPEG_HEADER_OK)
		return MLKERR_FORMAT_HEADER;

	//色空間

	colspace = jpg->jpeg_color_space;

	//CMYK 時

/*
	if(colspace == JCS_CMYK || colspace == JCS_YCCK)
	{
		//対応しない

		if(!(pli->flags & MLOADIMAGE_FLAGS_ALLOW_CMYK))
			return MLKERR_UNSUPPORTED;
	}
*/

	//mLoadImage にセット

	pli->width  = jpg->image_width;
	pli->height = jpg->image_height;
	pli->bits_per_sample = 8;

	//元のカラータイプ

	if(colspace == JCS_GRAYSCALE)
		coltype = MLOADIMAGE_COLTYPE_GRAY;
	else if(colspace == JCS_CMYK || colspace == JCS_YCCK)
		coltype = MLOADIMAGE_COLTYPE_CMYK;
	else
		coltype = MLOADIMAGE_COLTYPE_RGB;

	pli->src_coltype = coltype;

	//変換カラータイプ

	mLoadImage_setColorType_fromSource(pli);

	//------- 開始

	//出力フォーマット

	if(colspace == JCS_CMYK || colspace == JCS_YCCK)
		jpg->out_color_space = JCS_CMYK;
	else if(colspace == JCS_GRAYSCALE)
		jpg->out_color_space = JCS_GRAYSCALE;
	else
		jpg->out_color_space = JCS_RGB;

	//展開開始

	if(!jpeg_start_decompress(jpg))
		return MLKERR_ALLOC;

	//----- マーカー情報

	//解像度

	if(jpg->saw_JFIF_marker)
	{
		//JFIF
		pli->reso_horz = jpg->X_density;
		pli->reso_vert = jpg->Y_density;
		pli->reso_unit = _get_JFIF_resounit(jpg->density_unit);
	}
	else
		//EXIF
		_get_exif_resolution(p, pli);

	return MLKERR_OK;
}


//=================================
// main
//=================================


/* 終了 */

static void _jpeg_close(mLoadImage *pli)
{
	jpegdata *p = (jpegdata *)pli->handle;

	if(p)
	{
		jpeg_destroy_decompress(&p->jpg);

		mFree(p->rowbuf);

		if(p->fp && p->close_fp)
			fclose(p->fp);
	}

	mLoadImage_closeHandle(pli);
}

/* 開く */

static mlkerr _jpeg_open(mLoadImage *pli)
{
	int ret;

	//作成

	ret = mLoadImage_createHandle(pli, sizeof(jpegdata), -1);
	if(ret) return ret;

	//情報読み込み

	return _read_info((jpegdata *)pli->handle, pli);
}

/* イメージ読み込み */

static mlkerr _jpeg_getimage(mLoadImage *pli)
{
	jpegdata *p = (jpegdata *)pli->handle;
	uint8_t **ppdst,*rowbuf;
	mFuncImageConv convfunc;
	mImageConv conv;
	int i,height,prog,prog_cur;

	//バッファ確保

	i = pli->width;

	if(pli->src_coltype == MLOADIMAGE_COLTYPE_CMYK)
		i *= 4;
	else if(pli->src_coltype == MLOADIMAGE_COLTYPE_RGB)
		i *= 3;

	rowbuf = p->rowbuf = (uint8_t *)mMalloc(i);
	if(!rowbuf) return MLKERR_ALLOC;

	//変換関数

	mLoadImage_setImageConv(pli, &conv);

	conv.srcbuf = rowbuf;

	if(pli->src_coltype == MLOADIMAGE_COLTYPE_CMYK)
	{
		//CMYK -> CMYK/RGB/RGBA
		
		conv.flags |= MIMAGECONV_FLAGS_REVERSE;
		convfunc = mImageConv_cmyk8;
	}
	else if(pli->src_coltype == MLOADIMAGE_COLTYPE_GRAY)
		//GRAY -> GRAY/RGB/RGBA
		convfunc = mImageConv_gray8;
	else
		//RGB -> RGB/RGBA
		convfunc = mImageConv_rgb8;

	//読み込み

	ppdst = pli->imgbuf;
	height = p->jpg.output_height;
	prog_cur = 0;

	for(i = 0; i < height; i++)
	{
		jpeg_read_scanlines(&p->jpg, (JSAMPARRAY)&rowbuf, 1);

		conv.dstbuf = *ppdst;
		(convfunc)(&conv);

		ppdst++;

		//進捗

		if(pli->progress)
		{
			prog = (i + 1) * 100 / height;

			if(prog != prog_cur)
			{
				prog_cur = prog;
				(pli->progress)(pli, prog);
			}
		}
	}
	
	//終了

	jpeg_finish_decompress(&p->jpg);

	mFree(rowbuf);
	p->rowbuf = NULL;

	return (p->jpgerr.ferr)? MLKERR_DECODE: MLKERR_OK;
}

/**@ JPEG 判定と関数セット */

mlkbool mLoadImage_checkJPEG(mLoadImageType *p,uint8_t *buf,int size)
{
	if(!buf || (size >= 2 && buf[0] == 0xff && buf[1] == 0xd8))
	{
		p->format_tag = MLK_MAKE32_4('J','P','E','G');
		p->open = _jpeg_open;
		p->getimage = _jpeg_getimage;
		p->close = _jpeg_close;
		
		return TRUE;
	}

	return FALSE;
}

