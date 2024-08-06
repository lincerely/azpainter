#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/utsname.h>

#define CONFIG_X11  1
#define CONFIG_NASM 0

typedef struct
{
	char *buf;
	uint32_t len,size;
}string;

const char *prefix_str = "/usr/local", *cc_str = "cc";

string str_cflags, str_ldflags, str_libs;
int opt_debug = 0, os_type = 0;

#if defined(__x86_64__)
int enable_nasm = 1;
#else
int enable_nasm = 0;
#endif


enum
{
	OS_OTHER,
	OS_MAC
};


/* (string) 解放 */

static void str_free(string *p)
{
	if(p->buf) free(p->buf);
}

/* (string) バッファ確保 */

static int str_alloc(string *p)
{
	p->buf = (char *)malloc(64);
	if(!p->buf) return 1;
	
	p->buf[0] = 0;
	p->size = 64;
	p->len = 0;

	return 0;
}

/* (string) 空か */

static int str_is_empty(string *p)
{
	return (!p->buf || !p->len);
}

/* (string) リサイズ */

static int str_resize(string *p,int len)
{
	int size;
	char *buf;

	for(size = 64; len >= size; size <<= 1)
	{
		if(size == (1<<12)) return 0;
	}

	if(p->buf && p->size >= size) return 1;

	buf = (char *)realloc(p->buf, size);
	if(!buf) return 0;

	p->buf = buf;
	p->size = size;

	return 1;
}

/* (string) 空にする */

static void str_empty(string *p)
{
	if(p->buf) p->buf[0] = 0;
	p->len = 0;
}

/* (string) テキストセット */

static void str_settext(string *p,const char *txt)
{
	int len = strlen(txt);

	if(str_resize(p, len))
	{
		memcpy(p->buf, txt, len + 1);
		p->len = len;
	}
}

/* (string) テキスト追加 */

static void str_addtext(string *p,const char *txt)
{
	int len = strlen(txt);

	if(len && str_resize(p, p->len + len))
	{
		memcpy(p->buf + p->len, txt, len + 1);
		p->len += len;
	}
}

/* (string) テキスト追加 */

static void str_addtext_len(string *p,const char *txt,int len)
{
	if(len && str_resize(p, p->len + len))
	{
		memcpy(p->buf + p->len, txt, len);

		p->len += len;
		p->buf[p->len] = 0;
	}
}

/* 末尾の改行を削除 */

static void str_remove_last_enter(string *p)
{
	int pos;

	if(p->buf && p->len)
	{
		pos = p->len - 1;

		if(p->buf[pos] == '\n') pos--;
		if(p->buf[pos] == '\r') pos--;

		pos++;
		p->buf[pos] = 0;
		p->len = pos;
	}
}

/* 文字列検索 */

static int _search_str(const char *src,int srclen,const char *str,int len)
{
	while(srclen >= len)
	{
		if(*src == *str && strncmp(src, str, len) == 0)
			return 1;

		src++;
		srclen--;
	}

	return 0;
}

/* (string) テキスト追加
 *
 * 空白で区切った単語ごとに処理し、p に存在していないものだけ追加する。 */

static void str_addtext_sp(string *p,const char *txt)
{
	const char *end;
	int len;

	if(!p->buf) return;

	while(1)
	{
		//空白をスキップ

		for(; *txt && *txt == ' '; txt++);

		if(!(*txt)) break;

		//空白または終端までの単語

		for(end = txt + 1; *end && *end != ' '; end++);

		len = end - txt;

		//追加

		if(!_search_str(p->buf, p->len, txt, len))
		{
			if(p->len) str_addtext(p, " ");
			str_addtext_len(p, txt, len);
		}

		txt = end;
	}
}

/* 文字列の置き換え
 *
 * dst: NULL の場合、空文字列 */

static void _replace_text(string *p,const char *src,const char *dst)
{
	char *pc;
	int slen,dlen,diff,pos,newlen;

	if(str_is_empty(p)) return;

	if(!dst) dst = "";

	pc = p->buf;

	while(1)
	{
		pc = strstr(pc, src);
		if(!pc) break;

		pos = pc - p->buf;
		slen = strlen(src);
		dlen = strlen(dst);
		diff = dlen - slen;
		newlen = p->len + diff;

		if(diff == 0)
			memcpy(pc, dst, dlen);
		else
		{
			if(diff > 0 && newlen >= p->size)
			{
				if(!str_resize(p, newlen)) return;
			}

			memmove(pc + dlen, pc + slen, p->len - (pos + slen));
			memcpy(pc, dst, dlen);

			p->buf[newlen] = 0;
		}

		p->len = newlen;

		pc += slen;
	}
}

/* top 〜 end の範囲を削除 */

static void _delete_area(string *p,const char *top,const char *end)
{
	char *ptop,*pend;

	if(str_is_empty(p)) return;

	ptop = strstr(p->buf, top);
	if(!ptop) return;
	
	pend = strstr(ptop, end);
	if(!pend) return;

	pend += strlen(end);

	memmove(ptop, pend, p->buf + p->len - pend);

	p->len -= pend - ptop;
	p->buf[p->len] = 0;
}

/* top 〜 行末までを削除 */

static void _delete_line(string *p,const char *top)
{
	char *ptop,*pend,c;

	if(str_is_empty(p)) return;

	ptop = strstr(p->buf, top);
	if(!ptop) return;

	pend = ptop;
	while(*pend)
	{
		c = *(pend++);
		if(c == '\n')
			break;
		else if(c == '\r')
		{
			if(*pend == '\n') pend++;
			break;
		}
	}

	//削除

	memmove(ptop, pend, p->buf + p->len - pend);

	p->len -= pend - ptop;
	p->buf[p->len] = 0;
}

/* テキストファイル読み込み */

static void _read_file(string *dst,const char *path)
{
	FILE *fp;
	char *buf;
	long size;

	dst->buf = NULL;

	fp = fopen(path, "rb");
	if(!fp) return;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	dst->size = size + 2048;
	dst->len = size;

	dst->buf = buf = (char *)malloc(dst->size);
	if(buf)
	{
		fread(buf, 1, size, fp);
		buf[size] = 0;
	}

	fclose(fp);
}

/* テキストファイルに書き込み */

static void _write_file(string *p,const char *path)
{
	FILE *fp;

	if(!p->buf) return;

	fp = fopen(path, "wb");
	if(!fp) return;

	fwrite(p->buf, 1, p->len, fp);

	fclose(fp);
}


//=================


/* 終了 */

static void _app_exit(int ret)
{
	str_free(&str_cflags);
	str_free(&str_ldflags);
	str_free(&str_libs);

	exit(ret);
}

/* without オプション */

static void _opt_without(const char *name)
{
}

/* オプション処理 */

static void _proc_options(int argc,char **argv)
{
	int i;
	char *opt;

	for(i = 1; i < argc; i++)
	{
		opt = argv[i];

		if(strncmp(opt, "--without-", 10) == 0)
			_opt_without(opt + 10);
		else if(strncmp(opt, "--prefix=", 9) == 0)
			prefix_str = opt + 9;
		else if(strcmp(opt, "--debug") == 0)
			opt_debug = 1;
	#if CONFIG_NASM
		else if(strcmp(opt, "--enable-nasm") == 0)
			enable_nasm = 1;
		else if(strcmp(opt, "--disable-nasm") == 0)
			enable_nasm = 0;
	#endif
		else if(strncmp(opt, "CC=", 3) == 0)
			cc_str = opt + 3;
		else if(strncmp(opt, "CFLAGS=", 7) == 0)
			str_settext(&str_cflags, opt + 7);
		else if(strncmp(opt, "LDFLAGS=", 8) == 0)
			str_settext(&str_ldflags, opt + 8);
		else if(strncmp(opt, "LIBS=", 5) == 0)
			str_settext(&str_libs, opt + 5);
	}
}

/* OS 判定 */

static void _check_os(void)
{
	struct utsname n;

	if(uname(&n) == 0)
	{
		if(strcmp(n.sysname, "Darwin") == 0)
			os_type = OS_MAC;
	}
}

/* コマンドを実行して、stdout を文字列で取得 */

static int _run_command(string *str,const char *cmd)
{
	FILE *fp;
	char buf[64];
	int n;

	if(!cmd) return -1;

	str_empty(str);

	fflush(stdout);

	fp = popen(cmd, "r");
	if(!fp) return -1;

	while(!feof(fp))
	{
		n = fread(buf, 1, 64, fp);

		if(str_resize(str, str->len + n))
		{
			memcpy(str->buf + str->len, buf, n);
			str->len += n;
		}
	}

	str->buf[str->len] = 0;

	str_remove_last_enter(str);

	return pclose(fp);
}

/* ライブラリのチェック */

static void _check_lib(const char *name,const char *version,const char *help)
{
	string str,str2;

	str_alloc(&str);
	str_alloc(&str2);

	printf("checking %s...\n", name);

	//libs

	str_settext(&str, "pkg-config --libs \"");
	str_addtext(&str, name);
	if(version)
	{
		str_addtext(&str, " >= ");
		str_addtext(&str, version);
	}
	str_addtext(&str, "\"");

	if(!_run_command(&str2, str.buf))
		str_addtext_sp(&str_libs, str2.buf);
	else
	{
		printf("----- Error ----\n%s not found\nplease install '%s'\n", name, help);
		_app_exit(1);
	}

	//cflags

	str_settext(&str, "pkg-config --cflags ");
	str_addtext(&str, name);

	if(!_run_command(&str2, str.buf))
		str_addtext_sp(&str_cflags, str2.buf);

	//

	str_free(&str);
	str_free(&str2);
}

/* 動的ライブラリ&指定関数の存在チェック
 * lib: '.' 以降の拡張子は含めない
 * func: NULL でライブラリのチェックのみ
 * return: 1 で存在する */

static int _check_dynamic_lib(const char *lib,const char *func)
{
	string str;
	void *h;
	int ret;

	if(str_alloc(&str)) return 1;

	str_settext(&str, lib);
	str_addtext(&str, ".");
	str_addtext(&str, (os_type == OS_MAC)? "dylib": "so");

	h = dlopen(str.buf, RTLD_LAZY);
	if(!h) return 0;

	if(func)
		ret = (dlsym(h, func) != 0);
	else
		ret = 1;

	dlclose(h);

	return ret;
}

/* libc に関数が含まれているかチェック */

static int _check_libc_func(const char *name)
{
	return (dlsym(RTLD_NEXT, name) != 0);
}

static const uint16_t g_endian_val = 0x1234;

/* config.h */

static void _proc_config_h(void)
{
	string str;

	_read_file(&str, "config.h.in");
	if(!str.buf) return;

#if 1
	if(*((uint8_t *)&g_endian_val) == 0x12)
		_replace_text(&str, "#undef MLK_BIG_ENDIAN", "#define MLK_BIG_ENDIAN");
#endif

#if CONFIG_NASM
	if(enable_nasm)
		_replace_text(&str, "#undef -", "#define -");
#endif

	if(!_check_dynamic_lib("libfreetype", "FT_Done_MM_Var"))
		_replace_text(&str, "#undef MLK_NO_FREETYPE_VARIABLE", "#define MLK_NO_FREETYPE_VARIABLE");


	_write_file(&str, "build/config.h");

	str_free(&str);
}

/* ビルドファイル:行と範囲置き換え
 *
 * flag: 1 で削除、0 で残す */

static void _buildfile_rep(string *str,int flag,const char *txt1,const char *txt2,const char *txt3)
{
	if(flag)
	{
		_delete_line(str, txt1);
		_delete_area(str, txt2, txt3);
	}
	else
	{
		_replace_text(str, txt1, 0);
		_replace_text(str, txt2, 0);
		_replace_text(str, txt3, 0);
	}
}

/* ビルドファイル */

static void _proc_buildfile(const char *input,const char *output)
{
	string str;

	_read_file(&str, input);
	if(!str.buf) return;

	_replace_text(&str, "@PREFIX@", prefix_str);
	_replace_text(&str, "@CC@", cc_str);
	_replace_text(&str, "@CFLAGS@", str_cflags.buf);
	_replace_text(&str, "@LDFLAGS@", str_ldflags.buf);
	_replace_text(&str, "@LIBS@", str_libs.buf);
	_replace_text(&str, "@PACKAGE_NAME@", "azpainter");
	_replace_text(&str, "@PACKAGE_VERSION@", "3.0.9");

#if CONFIG_NASM
	_buildfile_rep(&str, !enable_nasm, "@ASM_x86_64:", "@ASM_x86_64>", "@ASM_x86_64<");
#endif

	_replace_text(&str, "@NASM_FMT@", (os_type == OS_MAC)? "macho64 --prefix _": "elf64");


	_write_file(&str, output);

	str_free(&str);
}

/* コマンドが存在するか */

static int _check_command(const char *name)
{
	string str;
	int ret;

	if(str_alloc(&str)) return 0;

	str_settext(&str, "type ");
	str_addtext(&str, name);
	str_addtext(&str, " > /dev/null 2>&1");

	ret = system(str.buf);

	str_free(&str);

	return (ret == 0);
}

/* main */

int main(int argc,char **argv)
{
	str_alloc(&str_cflags);
	str_alloc(&str_ldflags);
	str_alloc(&str_libs);

	//OS 判定

	_check_os();

	if(os_type == OS_MAC) enable_nasm = 0;

	//オプション処理

	_proc_options(argc, argv);

	//デフォルトのフラグ

	if(str_is_empty(&str_cflags))
		str_settext(&str_cflags, (opt_debug)? "-g": "-O2");

	str_addtext_sp(&str_cflags, "-I/usr/local/include");
	str_addtext_sp(&str_ldflags, "-L/usr/local/lib");

	//macOS (X11)

#if CONFIG_X11
	if(os_type == OS_MAC)
	{
		str_addtext_sp(&str_cflags, "-I/opt/X11/include");
		str_addtext_sp(&str_ldflags, "-L/opt/X11/lib");
		str_addtext_sp(&str_libs, "-lX11 -lXext -lXcursor -lXi");
	}
#endif

	//nasm (x86-64)

#if CONFIG_NASM
	if(enable_nasm)
	{
		if(_check_command("nasm"))
			printf("enable nasm x86-64\n");
		else
		{
			printf("[warning] 'nasm' not installed\n");
			enable_nasm = 0;
		}
	}
#endif

	//ライブラリ

	printf("checking iconv...");fflush(stdout);
	if(_check_dynamic_lib("libiconv", 0))
	{
		str_addtext_sp(&str_libs, "-liconv");
		printf("libiconv\n");
	}
	else if(_check_libc_func("iconv_open"))
		printf("libc\n");
	else
	{
		printf("\n[!] 'iconv' not found\nplease install 'libiconv'\n");
		_app_exit(1);
	}

	if(os_type != OS_MAC) _check_lib("x11", 0, "libx11-dev or libX11-devel or libx11");
	if(os_type != OS_MAC) _check_lib("xext", 0, "libxext-dev or libXext-devel or libxext");
	if(os_type != OS_MAC) _check_lib("xcursor", 0, "libxcursor-dev or libXcursor-devel or libxcursor");
	if(os_type != OS_MAC) _check_lib("xi", 0, "libxi-dev or libXi-devel or libxi");
	_check_lib("libpng", 0, "libpng-dev or libpng-devel or libpng or png");
	_check_lib("zlib", 0, "zlib1g-dev or zlib-devel or zlib");
	_check_lib("libtiff-4", 0, "libtiff-dev or libtiff-devel or libtiff or tiff");
	_check_lib("libjpeg", 0, "libjpeg-dev or libjpeg-devel or libjpeg-turbo or jpeg-turbo");
	_check_lib("libwebp", 0, "libwebp-dev or libwebp-devel or libwebp or webp");
	_check_lib("freetype2", 0, "libfreetype6-dev or libfreetype6-devel or freetype2");
	_check_lib("fontconfig", 0, "libfontconfig1-dev or libfontconfig-devel or fontconfig");

	//config.h

#if 1
	_proc_config_h();
#endif

	//ビルドファイル

	_proc_buildfile("build.ninja.in", "build/build.ninja");
	_proc_buildfile("install.sh.in", "build/install.sh");

	//結果

	printf("---------------\n"
"\n"
"prefix = %s\n"
"CFLAGS = %s\n"
"LDFLAGS = %s\n"
"LIBS = %s\n"
"\n"
"please run...\n"
"$ cd build\n"
"$ ninja\n"
"# ninja install\n"
"---------------\n"
, prefix_str, str_cflags.buf, str_ldflags.buf, str_libs.buf);

	_app_exit(0);

	return 0;
}
