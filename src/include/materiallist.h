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

/************************************
 * 素材画像管理リスト
 ************************************/

typedef struct _ImageMaterial ImageMaterial;

enum
{
	//getImage
	MATERIALLIST_TYPE_LAYER_TEXTURE = 0,
	MATERIALLIST_TYPE_BRUSH_TEXTURE,
	MATERIALLIST_TYPE_BRUSH_SHAPE,

	//releaseImage
	MATERIALLIST_TYPE_TEXTURE = 0,
	MATERIALLIST_TYPE_BRUSH
};


void MaterialList_init(mList *arr);

ImageMaterial *MaterialList_getImage(mList *arr,int type,const char *path,mlkbool keep);
void MaterialList_releaseImage(mList *arr,int type,ImageMaterial *img);

