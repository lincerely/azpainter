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

/*****************************************
 * AppDraw: キャンバス関連
 *****************************************/

#include <mlk_gui.h>
#include <mlk_widget.h>

#include "def_macro.h"
#include "def_config.h"
#include "def_draw.h"
#include "def_widget.h"

#include "maincanvas.h"
#include "panel_func.h"

#include "draw_main.h"
#include "draw_calc.h"



/** キャンバスの描画を低品質に */

void drawCanvas_lowQuality(void)
{
	APPDRAW->canvas_lowquality = 1;
}

/** キャンバスの描画品質を元に戻す */

void drawCanvas_normalQuality(void)
{
	APPDRAW->canvas_lowquality = 0;
}

/** スクロール位置をリセット (値のみ)
 *
 * イメージ原点位置をセットして、スクロール位置を (0,0) にする。
 *
 * origin: イメージ原点位置。NULL で、現在のキャンバス中央。 */

void drawCanvas_scroll_reset(AppDraw *p,const mDoublePoint *origin)
{
	if(origin)
		p->imgorigin = *origin;
	else
		drawCalc_getImagePos_atCanvasCenter(p, &p->imgorigin.x, &p->imgorigin.y);

	p->canvas_scroll.x = p->canvas_scroll.y = 0;
}

/** 指定イメージ位置がキャンバスの中心に来るようにスクロール */

void drawCanvas_scroll_at_imagecenter(AppDraw *p,const mDoublePoint *dpt)
{
	mPoint pt;

	drawCalc_image_to_canvas_pt_double(p, &pt, dpt->x, dpt->y);

	p->canvas_scroll.x = pt.x + p->canvas_scroll.x - p->canvas_size.w / 2;
	p->canvas_scroll.y = pt.y + p->canvas_scroll.y - p->canvas_size.h / 2;

	MainCanvas_setScrollPos();

	drawCanvas_update_scrollpos(p, TRUE);
}

/** スクロール位置をデフォルトに (画像の中央を原点位置とする) */

void drawCanvas_scroll_default(AppDraw *p)
{
	p->imgorigin.x = p->imgw * 0.5;
	p->imgorigin.y = p->imgh * 0.5;
	p->canvas_scroll.x = p->canvas_scroll.y = 0;
}

/** 表示倍率を一段階上げ下げ
 *
 * is_ptdev: ポインタデバイスによる操作か */

void drawCanvas_zoomStep(AppDraw *p,mlkbool zoomup,mlkbool is_ptdev)
{
	uint32_t flags;

	flags = DRAWCANVAS_UPDATE_ZOOM | DRAWCANVAS_UPDATE_RESET_SCROLL;

	if(is_ptdev && (g_app_config->foption & CONFIG_OPTF_ZOOM_AT_CURSOR))
		flags |= DRAWCANVAS_UPDATE_SCROLL_AT_CURSOR;

	drawCanvas_update(p, drawCalc_getZoom_step(p, zoomup), 0, flags);
}

/** 一段階左右に回転 */

void drawCanvas_rotateStep(AppDraw *p,mlkbool left)
{
	int step;

	step = APPCONF->canvas_angle_step;
	if(left) step = -step;

	drawCanvas_update(p, 0, p->canvas_angle + step * 100,
		DRAWCANVAS_UPDATE_ANGLE | DRAWCANVAS_UPDATE_RESET_SCROLL);
}

/** キャンバスをウィンドウに合わせる
 *
 * イメージ全体がキャンバス内に収まるように倍率セット。 */

void drawCanvas_fitWindow(AppDraw *p)
{
	drawCanvas_scroll_default(p);

	drawCanvas_update(p,
		drawCalc_getZoom_fitWindow(p), 0,
		DRAWCANVAS_UPDATE_ZOOM | DRAWCANVAS_UPDATE_ANGLE);
}

/** キャンバス左右反転表示切り替え */

void drawCanvas_mirror(AppDraw *p)
{
	mDoublePoint pt;

	drawCalc_getImagePos_atCanvasCenter(p, &pt.x, &pt.y);

	p->canvas_mirror = !p->canvas_mirror;

	//切り替え前の位置でリセット
	drawCanvas_scroll_reset(p, &pt);

	drawCanvas_update(p, 0, -(p->canvas_angle), DRAWCANVAS_UPDATE_ANGLE);
}

/** キャンバスの領域サイズ変更時 */

void drawCanvas_setCanvasSize(AppDraw *p,int w,int h)
{
	p->canvas_size.w = w;
	p->canvas_size.h = h;

	//スクロール情報変更

	MainCanvas_setScrollInfo();

	//定規ガイド再計算

	if(p->rule.func_set_guide)
		(p->rule.func_set_guide)(p);
}

/** スクロール位置のみが変更された時の更新 */

void drawCanvas_update_scrollpos(AppDraw *p,mlkbool update)
{
	//定規ガイド再計算

	if(p->rule.func_set_guide)
		(p->rule.func_set_guide)(p);

	//

	if(update)
		drawUpdate_canvas();
}

/** キャンバスの更新
 *
 * [!] キャンバスのスクロールバー範囲が変更される。 */

void drawCanvas_update(AppDraw *p,int zoom,int angle,int flags)
{
	mPoint pt,pt2;
	mDoublePoint dpt;
	int at_cursor = FALSE;

	//同時にズームを行う時、指定されたキャンバス位置のイメージが動かないように、スクロール調整

	if(flags & DRAWCANVAS_UPDATE_SCROLL_AT_CURSOR)
	{
		if(flags & DRAWCANVAS_UPDATE_SCROLL_AT_CURSOR_PRESS)
		{
			//ボタン押し時の位置 (ドラッグ操作時)
			at_cursor = TRUE;
			pt.x = (int)p->w.dpt_canv_press.x;
			pt.y = (int)p->w.dpt_canv_press.y;
			dpt = p->w.ptd_tmp[0];
		}
		else if(mWidgetGetCursorPos(MLK_WIDGET(g_app_widgets->canvaspage), &pt))
		{
			//現在のカーソル位置
			at_cursor = TRUE;
			drawCalc_canvas_to_image_double_pt(p, &dpt, &pt);
		}
	}

	//スクロールリセット
	// スクロール位置が (0,0) の場合は、そのまま

	if((flags & DRAWCANVAS_UPDATE_RESET_SCROLL)
		&& (p->canvas_scroll.x || p->canvas_scroll.y))
		drawCanvas_scroll_reset(p, NULL);

	//表示倍率

	if(flags & DRAWCANVAS_UPDATE_ZOOM)
	{
		if(zoom < CANVAS_ZOOM_MIN)
			zoom = CANVAS_ZOOM_MIN;
		else if(zoom > CANVAS_ZOOM_MAX)
			zoom = CANVAS_ZOOM_MAX;

		p->canvas_zoom = zoom;
	}

	//角度

	if(flags & DRAWCANVAS_UPDATE_ANGLE)
	{
		if(angle < -18000)
			angle += 36000;
		else if(angle > 18000)
			angle -= 36000;

		p->canvas_angle = angle;
	}

	//CanvasViewParam セット

	drawCalc_setCanvasViewParam(p);

	//イメージ位置が移動しないように、差分をスクロールする

	if(at_cursor)
	{
		drawCalc_image_to_canvas_pt_double(p, &pt2, dpt.x, dpt.y);

		p->canvas_scroll.x += pt2.x - pt.x;
		p->canvas_scroll.y += pt2.y - pt.y;
	}

	//定規ガイド再計算

	if(p->rule.func_set_guide)
		(p->rule.func_set_guide)(p);

	//キャンバススクロール情報

	MainCanvas_setScrollInfo();

	if(at_cursor)
		MainCanvas_setScrollPos();

	//ほか、関連するウィジェット

	PanelCanvasCtrl_setValue();

	//キャンバス更新

	if(!(flags & DRAWCANVAS_UPDATE_NO_CANVAS_UPDATE))
		drawUpdate_canvas();
}
