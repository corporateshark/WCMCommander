/*
   Copyright (c) by Valery Goryachev (Wal)
*/

#define _CRT_SECURE_NO_DEPRECATE  1
//#include "stdafx.h"
#ifdef MFC_VER
#include <afx.h>
#include <Afxwin.h>
#else
#if defined _WIN32 && !defined __GNUC__
#include <new.h>
#else
#include <new>
using namespace std;
#endif
#endif

#include "wal.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>


namespace wal
{


#ifdef _WIN32
#	define vsnprintf _vsnprintf
#endif

#if defined(_WIN32) && !defined( __GNUC__ )
	static int my_new_handler( size_t )
	{
		throw_oom();
		return 0;
	}
#else
	static void my_new_handler(  )
	{
		throw_oom();
	}
#endif


	void init_exceptions()
	{
#if defined _WIN32 && !defined __GNUC__
		_set_new_handler( my_new_handler );
#else
		set_new_handler( my_new_handler );
#endif
	}

	static coom oom;

	void cexception::destroy()
	{
		if ( this != &oom ) { delete this; }
	}

	const char* cexception::message()
	{
		return "exception";
	}

	cexception::~cexception() { }

	const char* coom::message()
	{
		return "out of memory";
	}

	coom::~coom() {}

	const char* cstop_exception::message() { return "stop"; }
	cstop_exception::~cstop_exception() {}

	cmsg::cmsg( const char* s )
	{
		if ( !s ) { return; }

		_msg.resize( strlen( s ) + 1 );
		strcpy( _msg.data(), s );
	}

	const char* cmsg::message()
	{
		return _msg.data() ? _msg.data() : "<?>";
	}

	cmsg::~cmsg() {}

	void throw_oom()
	{
		throw ( cexception* )( &oom );
	}

	void throw_stop()
	{
		throw  new cstop_exception();
	}


#define MAXMSGSTR 4096
	void throw_msg( const char* format, ... )
	{
		char buf[MAXMSGSTR];
		va_list ap;
		va_start( ap, format );
		vsnprintf( buf, MAXMSGSTR, format, ap );
		va_end( ap );
		cmsg* p = new cmsg( buf );
		throw p;
	}

	csyserr::csyserr( int _code, const char* msg )
		: cmsg( msg )
	{
		code = _code;
	}

	csyserr::~csyserr() {}

	void throw_syserr( int err, const char* format, ... )
	{
		char buf[MAXMSGSTR * 2] = "";

		if ( err == 0 ) { err = get_sys_error(); }

		if ( format != NULL )
		{
			va_list ap;
			va_start( ap, format );
			vsnprintf( buf, MAXMSGSTR, format, ap );
			va_end( ap );
		};

		strncat( buf, " (", MAXMSGSTR );

		strncat( buf, sys_error_utf8( err ).data(), MAXMSGSTR );

		strncat( buf, ")", MAXMSGSTR );

		buf[MAXMSGSTR - 1] = 0;

		csyserr* p = new csyserr( err, buf );

		throw p;
	}


}; //namespace wal
