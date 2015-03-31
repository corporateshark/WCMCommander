/*
   Copyright (c) by Valery Goryachev (Wal)
*/


#include "wal_sys_api.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <process.h>
#else
#include <stdlib.h>
#include <string.h>
#endif
#include <locale>
namespace wal
{

	static void sys_locale_init();

#ifdef _DEBUG

	void dbg_printf( const char* format, ... )
	{
		char s[1024];
		va_list ap;
		va_start( ap, format );
		Lvsnprintf( s, sizeof( s ), format, ap );
		va_end( ap );
#ifdef _WIN32
		OutputDebugString( s );
#else
		printf( "%s", s );
#endif
	}

#endif


#ifdef _WIN32

#define INFMETA 0x19701027

	struct _thread_info
	{
		unsigned meta;
		HANDLE handle;
		void* ( *f )( void* );
		void* data;
		void* ret;
	};

	static DWORD tlsId = TLS_OUT_OF_INDEXES;


	static /*void*/ unsigned __stdcall _thRun( void* a )
	{
		_thread_info* p = ( _thread_info* )a;

		if ( !TlsSetValue( tlsId, p ) )
		{
			;//???
		}

		try
		{
			p->ret = p->f( p->data );
		}
		catch ( ... )
		{
			p->ret = 0;
		}

		if ( !( p->handle ) ) { free( p ); }

		return 0;
	}

	int thread_create( thread_t* th, void* ( *f )( void* ), void* arg, bool detached )
	{
		if ( tlsId == TLS_OUT_OF_INDEXES )
		{
			tlsId = TlsAlloc();

			if ( tlsId == TLS_OUT_OF_INDEXES ) { return -1; }

			TlsSetValue( tlsId, 0 );
		}

		_thread_info* tinfo = ( _thread_info* )malloc( sizeof( _thread_info ) );

		if ( !tinfo )
		{
			SetLastError( ERROR_NOT_ENOUGH_MEMORY );
			return -1;
		};

		tinfo->meta = INFMETA;

		tinfo->f = f;

		tinfo->data = arg;

		tinfo->ret = 0;

		tinfo->handle = 0; //for detached - 0

		uintptr_t r = _beginthreadex( 0, 0, _thRun, tinfo, detached ? 0 : CREATE_SUSPENDED, 0 );

		if ( r == -1L )
		{
			free( tinfo );
			SetLastError( ERROR_TOO_MANY_TCBS );
			return -1;
		}

		if ( detached )
		{
			CloseHandle( ( HANDLE )r );

			if ( th ) { *th = 0; }

			return 0;
		}
		else
		{
			tinfo->handle = ( HANDLE )r;

			if ( th ) { *th = tinfo; }

			ResumeThread( ( HANDLE )r );
		}

		return 0;
	}

	thread_t thread_self()
	{
		if ( tlsId == TLS_OUT_OF_INDEXES ) { return 0; }

		return ( thread_t )TlsGetValue( tlsId );
	}

	int thread_join( thread_t th, void** val )
	{
		if ( !th || th->meta != INFMETA || th->handle == 0 )
		{
			SetLastError( ERROR_INVALID_PARAMETER );
			return -1;
		}

		if ( WaitForSingleObject( th->handle, INFINITE ) != WAIT_OBJECT_0 )
		{
			return -1;
		}

		if ( val ) { *val = th->ret; }

		CloseHandle( th->handle );
		free( th );
		return 0;
	}

	int cond_create( cond_t* c )
	{
		c->ev[0] = c->ev[1] = 0;

		if ( !( c->ev[0] = CreateEvent( NULL, TRUE, FALSE, NULL ) ) || !( c->ev[1] = CreateEvent( NULL, FALSE, FALSE, NULL ) ) )
		{
			DWORD e = GetLastError();

			if ( c->ev[0] ) { CloseHandle( c->ev[0] ); c->ev[0] = 0; }

			if ( c->ev[1] ) { CloseHandle( c->ev[1] ); c->ev[1] = 0; }

			SetLastError( e );
			return -1;
		}

		return 0;
	}

	int cond_delete( cond_t* c )
	{
		DWORD e0 = ( CloseHandle( c->ev[0] ) == S_OK ) ? 0 : GetLastError();
		DWORD e1 = ( CloseHandle( c->ev[1] ) == S_OK ) ? 0 : GetLastError();

		if ( e0 ) { SetLastError( e0 ); return -1; }

		if ( e1 ) { SetLastError( e1 ); return -1; }

		return 0;
	}

	int cond_wait( cond_t* c, mutex_t* m )
	{
		if ( mutex_unlock( m ) ) { return -1; }

		DWORD ret = WaitForMultipleObjects( 2, c->ev, FALSE, INFINITE );

		if ( mutex_lock( m ) ) { return -1; }

		if ( ret == WAIT_OBJECT_0 || ret == WAIT_OBJECT_0 + 1 ) { return 0; }

		return -1;
	}

	int cond_signal( cond_t* c ) { return SetEvent( c->ev[1] ) ? 0 : -1; }
	int cond_broadcast( cond_t* c ) {    return PulseEvent( c->ev[0] ) ? 0 : -1; };




//return count of characters (length of buffer for conversion to unicode)
	int sys_symbol_count( const sys_char_t* s, int len )
	{
		if ( len < 0 )
		{
			for ( len = 0; *s; s++ ) { len++; }
		}

		return len;
	}

//return count of sys_chars for saving s
	int sys_string_buffer_len( const unicode_t* s, int ulen ) //not including \0
	{
		if ( ulen < 0 )
		{
			for ( ulen = 0; *s; s++ ) { ulen++; }
		}

		return ulen;
	}

// return point of appended \0
	unicode_t* sys_to_unicode(
	   unicode_t* buf,
	   const sys_char_t* s, int len,
	   int* badCount )
	{
		if ( len < 0 )
		{
			for ( ; *s; s++, buf++ ) { *buf = *s; }
		}
		else
		{
			for ( int i = 0; i < len; i++, s++, buf++ ) { *buf = *s; }
		}

		*buf = 0;

		if ( badCount ) { *badCount = 0; }

		return buf;
	}

// return point of appended \0
	sys_char_t* unicode_to_sys(
	   sys_char_t* s,
	   const unicode_t* buf, int ulen,
	   int* badCount )
	{
		if ( ulen < 0 )
		{
			for ( ; *buf; s++, buf++ ) { *s = *buf; }
		}
		else
		{
			for ( int i = 0; i < ulen; i++, s++, buf++ ) { *s = *buf; }
		}

		*s = 0;

		if ( badCount ) { *badCount = 0; }

		return s;
	}

	sys_char_t* sys_error_str( int err, sys_char_t* buf, int size )
	{
		if ( err == 0 ) { err = GetLastError(); }

		if ( FormatMessageW(
		        FORMAT_MESSAGE_FROM_SYSTEM |
		        FORMAT_MESSAGE_IGNORE_INSERTS,
		        NULL,
		        err,
		        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), // Default language
		        buf,
		        size,
		        NULL
		     ) < 2 )
		{
#if defined( _MSC_VER )
			_snwprintf_s( buf, size, _TRUNCATE, L"Error code: %i", int( err ) );
#else
			_snwprintf( buf, size, L"Error code: %i", int( err ) );
#endif
		}

		if ( size > 0 ) { buf[size - 1] = 0; }

		for ( sys_char_t* s = buf; *s; s++ ) if ( *s == '\r' ) { *s = ' '; }

		return buf;
	}

	bool InitSystemLocales()
	{
		sys_locale_init();
		return true;
	}

#else

	int sys_charset_id = CS_LATIN1;

	extern unicode_t* latin1_to_unicode( unicode_t* buf, const char* s, int size, int* badCount );
	extern char* unicode_to_latin1( char* s, const unicode_t* buf, int usize, int* badCount );
	extern int cp8_symbol_count( const char* s, int size );
	extern int cp8_string_buffer_len( const unicode_t* buf, int ulen );

	unicode_t* ( *fp_sys_to_unicode )( unicode_t* buf, const sys_char_t* s, int size, int* badCount ) = latin1_to_unicode;
	sys_char_t* ( *fp_unicode_to_sys )( sys_char_t* s, const unicode_t* buf, int usize, int* badCount ) = unicode_to_latin1;
	int ( *fp_sys_symbol_count )( const sys_char_t* s, int size ) = cp8_symbol_count;
	int ( *fp_sys_string_buffer_len )( const unicode_t* s, int ulen ) = cp8_string_buffer_len;


	struct LacaleCharsetAlias
	{
		const char* alias;
		int id;
	};

	static LacaleCharsetAlias csAliasTable[] =
	{
		{"UTF-8",   CS_UTF8},
		{"CP1251",  CS_WIN1251},
		{"CP866",   CS_CP866},
		{"KOI8-R",  CS_KOI8R},
		{"KOI8R",   CS_KOI8R},
		{0, 0}
	};

	static void set_sys_charset( int id )
	{
		charset_struct* p = charset_table[id];
		fp_sys_to_unicode = p->cs_to_unicode;
		fp_unicode_to_sys = p->unicode_to_cs;
		fp_sys_symbol_count = p->symbol_count;
		fp_sys_string_buffer_len = p->string_buffer_len;
		sys_charset_id = id;
	};

	bool InitSystemLocales()
	{
		sys_locale_init();

		set_sys_charset( CS_UTF8 );

		char* s = getenv( "LANG" );

		if ( !s )
		{
			return true;
		}

		while ( *s && *s != '.' ) { s++; }

		if ( *s == '.' ) { s++; }

		for ( LacaleCharsetAlias* p = csAliasTable; p->alias; p++ )
			if ( !strcasecmp( s, p->alias ) )
			{
				set_sys_charset( p->id );
				return true;
			}


		printf( "'%s' (charset not found)\n", s );

		return true;
	}

	sys_char_t* sys_error_str( int err, sys_char_t* buf, int size )
	{
		if ( size > 0 )
		{
			buf[0] = 0;


#if defined(__APPLE__) || \
   defined(__FreeBSD__) || \
   defined(__OpenBSD__) || \
   defined(_POSIX_C_SOURCE) && defined(_XOPEN_SOURCE) && defined(_GNU_SOURCE) && (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
			strerror_r( err, buf, size );
#else
			return strerror_r( err, buf, size );
#endif

		}

		return buf;
	}
#endif

	static char locale_lang[32] = "";
	static char locale_ter[32] = "";
	static char locale_lang_ter[32] = "";
	static const char* locale_str = 0;

#ifdef _WIN32
	struct W32LCNode
	{
		int n;
		const char* lang;
	};

	static W32LCNode w32LCNode[] =
	{
		{0x0001, "ar"}, {0x0002, "bg"}, {0x0003, "ca"}, {0x0004, "zh_Hans"},  {0x0005, "cs"},
		{0x0006, "da"}, {0x0007, "de"}, {0x0008, "el"}, {0x0009, "en"}, {0x000a, "es"},
		{0x000b, "fi"}, {0x000c, "fr"}, {0x000d, "he"}, {0x000e, "hu"}, {0x000f, "is"},
		{0x0010, "it"}, {0x0011, "ja"}, {0x0012, "ko"}, {0x0013, "nl"}, {0x0014, "no"},
		{0x0015, "pl"}, {0x0016, "pt"}, {0x0017, "rm"}, {0x0018, "ro"}, {0x0019, "ru"},
		{0x001a, "bs"}, {0x001b, "sk"}, {0x001c, "sq"}, {0x001d, "sv"}, {0x001e, "th"},
		{0x001f, "tr"}, {0x0020, "ur"}, {0x0021, "id"}, {0x0022, "uk"}, {0x0023, "be"},
		{0x0024, "sl"}, {0x0025, "et"}, {0x0026, "lv"}, {0x0027, "lt"}, {0x0028, "tg"},
		{0x0029, "fa"}, {0x002a, "vi"}, {0x002b, "hy"}, {0x002c, "az"}, {0x002d, "eu"},
		{0x002e, "dsb"},   {0x002f, "mk"}, {0x0030, "st"}, {0x0031, "ts"}, {0x0032, "tn"},
		{0x0033, "ve"}, {0x0034, "xh"}, {0x0035, "zu"}, {0x0036, "af"}, {0x0037, "ka"},
		{0x0038, "fo"}, {0x0039, "hi"}, {0x003a, "mt"}, {0x003b, "se"}, {0x003c, "ga"},
		{0x003d, "yi"}, {0x003e, "ms"}, {0x003f, "kk"}, {0x0040, "ky"}, {0x0041, "sw"},
		{0x0042, "tk"}, {0x0043, "uz"}, {0x0044, "tt"}, {0x0045, "bn"}, {0x0046, "pa"},
		{0x0047, "gu"}, {0x0048, "or"}, {0x0049, "ta"}, {0x004a, "te"}, {0x004b, "kn"},
		{0x004c, "ml"}, {0x004d, "as"}, {0x004e, "mr"}, {0x004f, "sa"}, {0x0050, "mn"},
		{0x0051, "bo"}, {0x0052, "cy"}, {0x0053, "km"}, {0x0054, "lo"}, {0x0055, "my"},
		{0x0056, "gl"}, {0x0057, "kok"},   {0x0058, "mni"},   {0x0059, "sd"}, {0x005a, "syr"},
		{0x005b, "si"}, {0x005c, "chr"},   {0x005d, "iu"}, {0x005e, "am"}, {0x005f, "tzm"},
		{0x0060, "ks"}, {0x0061, "ne"}, {0x0062, "fy"}, {0x0063, "ps"}, {0x0064, "fil"},
		{0x0065, "dv"}, {0x0066, "bin"},   {0x0067, "ff"}, {0x0068, "ha"}, {0x0069, "ibb"},
		{0x006a, "yo"}, {0x006b, "quz"},   {0x006c, "nso"},   {0x006d, "ba"}, {0x006e, "lb"},
		{0x006f, "kl"}, {0x0070, "ig"}, {0x0071, "kr"}, {0x0072, "om"}, {0x0073, "ti"},
		{0x0074, "gn"}, {0x0075, "haw"},   {0x0076, "la"}, {0x0077, "so"}, {0x0078, "ii"},
		{0x0079, "pap"},   {0x007a, "arn"},   {0x007b, "Neither"},  {0x007c, "moh"},   {0x007d, "Neither"},
		{0x007e, "br"}, {0x007f, "Reserved"}, {0x0080, "ug"}, {0x0081, "mi"}, {0x0082, "oc"},
		{0x0083, "co"}, {0x0084, "gsw"},   {0x0085, "sah"},   {0x0086, "qut"},   {0x0087, "rw"},
		{0x0088, "wo"}, {0x0089, "Neither"},  {0x008a, "Neither"},  {0x008b, "Neither"},  {0x008c, "prs"},
		{0x008d, "Neither"},  {0x008e, "Neither"},  {0x008f, "Neither"},  {0x0090, "Neither"},  {0x0091, "gd"},
		{0x0092, "ku"}, {0x0093, "quc"},   {0x0401, "ar_SA"}, {0x0402, "bg_BG"}, {0x0403, "ca_ES"},
		{0x0404, "zh_TW"}, {0x0405, "cs_CZ"}, {0x0406, "da_DK"}, {0x0407, "de_DE"}, {0x0408, "el_GR"},
		{0x0409, "en_US"}, {0x040a, "es_ES_tradnl"},   {0x040b, "fi_FI"}, {0x040c, "fr_FR"}, {0x040d, "he_IL"},
		{0x040e, "hu_HU"}, {0x040f, "is_IS"}, {0x0410, "it_IT"}, {0x0411, "ja_JP"}, {0x0412, "ko_KR"},
		{0x0413, "nl_NL"}, {0x0414, "nb_NO"}, {0x0415, "pl_PL"}, {0x0416, "pt_BR"}, {0x0417, "rm_CH"},
		{0x0418, "ro_RO"}, {0x0419, "ru_RU"}, {0x041a, "hr_HR"}, {0x041b, "sk_SK"}, {0x041c, "sq_AL"},
		{0x041d, "sv_SE"}, {0x041e, "th_TH"}, {0x041f, "tr_TR"}, {0x0420, "ur_PK"}, {0x0421, "id_ID"},
		{0x0422, "uk_UA"}, {0x0423, "be_BY"}, {0x0424, "sl_SI"}, {0x0425, "et_EE"}, {0x0426, "lv_LV"},
		{0x0427, "lt_LT"}, {0x0428, "tg_Cyrl_TJ"},  {0x0429, "fa_IR"}, {0x042a, "vi_VN"}, {0x042b, "hy_AM"},
		{0x042c, "az_Latn_AZ"},  {0x042d, "eu_ES"}, {0x042e, "hsb_DE"},   {0x042f, "mk_MK"}, {0x0430, "st_ZA"},
		{0x0431, "ts_ZA"}, {0x0432, "tn_ZA"}, {0x0433, "ve_ZA"}, {0x0434, "xh_ZA"}, {0x0435, "zu_ZA"},
		{0x0436, "af_ZA"}, {0x0437, "ka_GE"}, {0x0438, "fo_FO"}, {0x0439, "hi_IN"}, {0x043a, "mt_MT"},
		{0x043b, "se_NO"}, {0x043d, "yi_Hebr"},  {0x043e, "ms_MY"}, {0x043f, "kk_KZ"}, {0x0440, "ky_KG"},
		{0x0441, "sw_KE"}, {0x0442, "tk_TM"}, {0x0443, "uz_Latn_UZ"},  {0x0444, "tt_RU"}, {0x0445, "bn_IN"},
		{0x0446, "pa_IN"}, {0x0447, "gu_IN"}, {0x0448, "or_IN"}, {0x0449, "ta_IN"}, {0x044a, "te_IN"},
		{0x044b, "kn_IN"}, {0x044c, "ml_IN"}, {0x044d, "as_IN"}, {0x044e, "mr_IN"}, {0x044f, "sa_IN"},
		{0x0450, "mn_MN"}, {0x0451, "bo_CN"}, {0x0452, "cy_GB"}, {0x0453, "km_KH"}, {0x0454, "lo_LA"},
		{0x0455, "my_MM"}, {0x0456, "gl_ES"}, {0x0457, "kok_IN"},   {0x0458, "mni_IN"},   {0x0459, "sd_Deva_IN"},
		{0x045a, "syr_SY"},   {0x045b, "si_LK"}, {0x045c, "chr_Cher_US"}, {0x045d, "iu_Cans_CA"},  {0x045e, "am_ET"},
		{0x045f, "tzm_Arab_MA"}, {0x0460, "ks_Arab"},  {0x0461, "ne_NP"}, {0x0462, "fy_NL"}, {0x0463, "ps_AF"},
		{0x0464, "fil_PH"},   {0x0465, "dv_MV"}, {0x0466, "bin_NG"},   {0x0467, "fuv_NG"},   {0x0468, "ha_Latn_NG"},
		{0x0469, "ibb_NG"},   {0x046a, "yo_NG"}, {0x046b, "quz_BO"},   {0x046c, "nso_ZA"},   {0x046d, "ba_RU"},
		{0x046e, "lb_LU"}, {0x046f, "kl_GL"}, {0x0470, "ig_NG"}, {0x0471, "kr_NG"}, {0x0472, "om_ET"},
		{0x0473, "ti_ET"}, {0x0474, "gn_PY"}, {0x0475, "haw_US"},   {0x0476, "la_Latn"},  {0x0477, "so_SO"},
		{0x0478, "ii_CN"}, {0x0479, "pap_029"},  {0x047a, "arn_CL"},   {0x047c, "moh_CA"},   {0x047e, "br_FR"},
		{0x0480, "ug_CN"}, {0x0481, "mi_NZ"}, {0x0482, "oc_FR"}, {0x0483, "co_FR"}, {0x0484, "gsw_FR"},
		{0x0485, "sah_RU"},   {0x0486, "qut_GT"},   {0x0487, "rw_RW"}, {0x0488, "wo_SN"}, {0x048c, "prs_AF"},
		{0x048d, "plt_MG"},   {0x048e, "zh_yue_HK"},   {0x048f, "tdd_Tale_CN"}, {0x0490, "khb_Talu_CN"}, {0x0491, "gd_GB"},
		{0x0492, "ku_Arab_IQ"},  {0x0493, "quc_CO"},   {0x0501, "qps_ploc"}, {0x05fe, "qps_ploca"},   {0x0801, "ar_IQ"},
		{0x0803, "ca_ES_valencia"}, {0x0804, "zh_CN"}, {0x0807, "de_CH"}, {0x0809, "en_GB"}, {0x080a, "es_MX"},
		{0x080c, "fr_BE"}, {0x0810, "it_CH"}, {0x0811, "ja_Ploc_JP"},  {0x0813, "nl_BE"}, {0x0814, "nn_NO"},
		{0x0816, "pt_PT"}, {0x0818, "ro_MD"}, {0x0819, "ru_MD"}, {0x081a, "sr_Latn_CS"},  {0x081d, "sv_FI"},
		{0x0820, "ur_IN"}, {0x0827, "Neither"},  {0x082c, "az_Cyrl_AZ"},  {0x082e, "dsb_DE"},   {0x0832, "tn_BW"},
		{0x083b, "se_SE"}, {0x083c, "ga_IE"}, {0x083e, "ms_BN"}, {0x0843, "uz_Cyrl_UZ"},  {0x0845, "bn_BD"},
		{0x0846, "pa_Arab_PK"},  {0x0849, "ta_LK"}, {0x0850, "mn_Mong_CN"},  {0x0851, "bo_BT"}, {0x0859, "sd_Arab_PK"},
		{0x085d, "iu_Latn_CA"},  {0x085f, "tzm_Latn_DZ"}, {0x0860, "ks_Deva"},  {0x0861, "ne_IN"}, {0x0867, "ff_Latn_SN"},
		{0x086b, "quz_EC"},   {0x0873, "ti_ER"}, {0x09ff, "qps_plocm"},   {0x0c01, "ar_EG"}, {0x0c04, "zh_HK"},
		{0x0c07, "de_AT"}, {0x0c09, "en_AU"}, {0x0c0a, "es_ES"}, {0x0c0c, "fr_CA"}, {0x0c1a, "sr_Cyrl_CS"},
		{0x0c3b, "se_FI"}, {0x0c50, "mn_Mong_MN"},  {0x0c5f, "tmz_MA"},   {0x0c6b, "quz_PE"},   {0x1001, "ar_LY"},
		{0x1004, "zh_SG"}, {0x1007, "de_LU"}, {0x1009, "en_CA"}, {0x100a, "es_GT"}, {0x100c, "fr_CH"},
		{0x101a, "hr_BA"}, {0x103b, "smj_NO"},   {0x105f, "tzm_Tfng_MA"}, {0x1401, "ar_DZ"}, {0x1404, "zh_MO"},
		{0x1407, "de_LI"}, {0x1409, "en_NZ"}, {0x140a, "es_CR"}, {0x140c, "fr_LU"}, {0x141a, "bs_Latn_BA"},
		{0x143b, "smj_SE"},   {0x1801, "ar_MA"}, {0x1809, "en_IE"}, {0x180a, "es_PA"}, {0x180c, "fr_MC"},
		{0x181a, "sr_Latn_BA"},  {0x183b, "sma_NO"},   {0x1c01, "ar_TN"}, {0x1c09, "en_ZA"}, {0x1c0a, "es_DO"},
		{0x1c0c, "Neither"},  {0x1c1a, "sr_Cyrl_BA"},  {0x1c3b, "sma_SE"},   {0x2001, "ar_OM"}, {0x2008, "Neither"},
		{0x2009, "en_JM"}, {0x200a, "es_VE"}, {0x200c, "fr_RE"}, {0x201a, "bs_Cyrl_BA"},  {0x203b, "sms_FI"},
		{0x2401, "ar_YE"}, {0x2409, "en_029"},   {0x240a, "es_CO"}, {0x240c, "fr_CD"}, {0x241a, "sr_Latn_RS"},
		{0x243b, "smn_FI"},   {0x2801, "ar_SY"}, {0x2809, "en_BZ"}, {0x280a, "es_PE"}, {0x280c, "fr_SN"},
		{0x281a, "sr_Cyrl_RS"},  {0x2c01, "ar_JO"}, {0x2c09, "en_TT"}, {0x2c0a, "es_AR"}, {0x2c0c, "fr_CM"},
		{0x2c1a, "sr_Latn_ME"},  {0x3001, "ar_LB"}, {0x3009, "en_ZW"}, {0x300a, "es_EC"}, {0x300c, "fr_CI"},
		{0x301a, "sr_Cyrl_ME"},  {0x3401, "ar_KW"}, {0x3409, "en_PH"}, {0x340a, "es_CL"}, {0x340c, "fr_ML"},
		{0x3801, "ar_AE"}, {0x3809, "en_ID"}, {0x380a, "es_UY"}, {0x380c, "fr_MA"}, {0x3c01, "ar_BH"},
		{0x3c09, "en_HK"}, {0x3c0a, "es_PY"}, {0x3c0c, "fr_HT"}, {0x4001, "ar_QA"}, {0x4009, "en_IN"},
		{0x400a, "es_BO"}, {0x4401, "ar_Ploc_SA"},  {0x4409, "en_MY"}, {0x440a, "es_SV"}, {0x4801, "ar_145"},
		{0x4809, "en_SG"}, {0x480a, "es_HN"}, {0x4c09, "en_AE"}, {0x4c0a, "es_NI"}, {0x5009, "en_BH"},
		{0x500a, "es_PR"}, {0x5409, "en_EG"}, {0x540a, "es_US"}, {0x5809, "en_JO"}, {0x580a, "es_419"},
		{0x5c09, "en_KW"}, {0x6009, "en_TR"}, {0x6409, "en_YE"}, {0x641a, "bs_Cyrl"},  {0x681a, "bs_Latn"},
		{0x6c1a, "sr_Cyrl"},  {0x701a, "sr_Latn"},  {0x703b, "smn"},      {0x742c, "az_Cyrl"},  {0x743b, "sms"},
		{0x7804, "zh"},    {0x7814, "nn"},    {0x781a, "bs"},    {0x782c, "az_Latn"},  {0x783b, "sma"},
		{0x7843, "uz_Cyrl"},  {0x7850, "mn_Cyrl"},  {0x785d, "iu_Cans"},  {0x785f, "tzm_Tfng"}, {0x7c04, "zh_Hant"},
		{0x7c14, "nb"},    {0x7c1a, "sr"},    {0x7c28, "tg_Cyrl"},  {0x7c2e, "dsb"},      {0x7c3b, "smj"},
		{0x7c43, "uz_Latn"},  {0x7c46, "pa_Arab"},  {0x7c50, "mn_Mong"},  {0x7c59, "sd_Arab"},  {0x7c5c, "chr_Cher"},
		{0x7c5d, "iu_Latn"},  {0x7c5f, "tzm_Latn"}, {0x7c67, "ff_Latn"},  {0x7c68, "ha_Latn"},  {0x7c92, "ku_Arab"},
		{0, 0}
	};


#endif

	static void sys_locale_init()
	{
		if ( !locale_str )
		{
#ifdef _WIN32
			unsigned lcid = GetUserDefaultLCID();

			for ( W32LCNode* p = w32LCNode; p->n && p->lang; p++ )
				if ( p->n == lcid )
				{
					locale_str = p->lang;
					break;
				};

#else
			locale_str = getenv( "LANG" );

#endif

			if ( !locale_str ) { locale_str = "en"; }

			if ( locale_str )
			{
				const char* s = locale_str;
				int i;

				for ( i = 0 ; i < sizeof( locale_lang ) - 1 && *s && *s != '_' && *s != '.'; i++, s++ ) { locale_lang[i] = *s; }

				locale_lang[i] = 0;

				for ( i = 0 ; i < sizeof( locale_ter ) - 1 && *s && *s != '.'; i++, s++ ) { locale_ter[i] = *s; }

				locale_ter[i] = 0;

				s = locale_str;

				for ( i = 0 ; i < sizeof( locale_lang_ter ) - 1 && *s && *s != '.'; i++, s++ ) { locale_lang_ter[i] = *s; }

				locale_lang_ter[i] = 0;
			}
		};
	}

	const char* sys_locale_lang() { return locale_lang; }
	const char* sys_locale_ter() { return locale_ter;}
	const char* sys_locale_lang_ter() { return locale_lang_ter; }
}; //namespace wal

