/*
 * Part of Wal Commander GitHub Edition
 * https://github.com/corporateshark/WalCommander
 * walcommander@linderdaum.com
 */

#ifdef _WIN32
#	include <winsock2.h>
#endif


#include "vfs-sftp.h"

#ifdef LIBSSH2_EXIST

#include "string-util.h"


void InitSSH()
{
	libssh2_init( 0 );
}


enum INT_SSH_ERRORS
{
   SSH_INTERROR_NOTSUPPORT = -20,
   SSH_INTERROR_X3 = -21,
   SSH_INTERROR_CONNECT = -22,
   SSH_INTERROR_AUTH = -23,
   SSH_INTERROR_FATAL = -24,
   SSH_INTERROR_OUTOF = -25,
   SSH_INTERROR_UNSUPPORTED_AUTH = -26,
   SSH_INTERROR_STOPPED = -50
};


FSSftp::FSSftp( FSSftpParam* param )
	:  FS( SFTP ), sshSession( 0 ), sftpSession( 0 )
{
	if ( param )
	{
		_operParam = *param;
		_infoParam = *param;
	}

	for ( int i = 0; i < MAX_FILES; i++ )
	{
		fileTable[i] = 0;
	}
}


void FSSftp::WaitSocket( FSCInfo* info ) //throw int(errno) or int(-2) on stop
{
	while ( true )
	{
		if ( info && info->Stopped() ) { throw int( -2 ); } //stopped

		int dir = libssh2_session_block_directions( sshSession );

		if ( ( dir & ( LIBSSH2_SESSION_BLOCK_INBOUND | LIBSSH2_SESSION_BLOCK_OUTBOUND ) ) == 0 )
		{
			return ;
		}

		int n = _sock.Select2(
		           ( dir & LIBSSH2_SESSION_BLOCK_INBOUND ) != 0,
		           ( dir & LIBSSH2_SESSION_BLOCK_OUTBOUND ) != 0,
		           3 );

		if ( n > 0 ) { return; }
	}
}


#define WHILE_EAGAIN_(retvar, a) \
while (true) {\
   retvar = a;\
   if (retvar != LIBSSH2_ERROR_EAGAIN) break;\
   WaitSocket(info);\
}

void FSSftp::CheckSessionEagain()
{
	int e = libssh2_session_last_errno( this->sshSession );

	if ( e != LIBSSH2_ERROR_EAGAIN ) { throw int( e - 1000 ); }
}

inline int TransSftpError( int e, LIBSSH2_SFTP* sftp )
{
	if ( e == LIBSSH2_ERROR_SFTP_PROTOCOL ) { e = libssh2_sftp_last_error( sftp ); }

	return e < 0 ? e - 1000 : e;
}

void FSSftp::CheckSFTPEagain()
{
	int e = libssh2_session_last_errno( sshSession );

	if ( e == LIBSSH2_ERROR_EAGAIN ) { return; }

	throw TransSftpError( e, sftpSession );
}

inline void FSSftp::CheckSFTP( int err )
{
	if ( !err ) { return; }

	throw TransSftpError( err, sftpSession );
}


static std::vector<char> CopyToStrZ( const char* s, int size )
{
	if ( size <= 0 ) { size = 0; }

	std::vector<char> p( size + 1 );

	if ( size > 0 ) { memcpy( p.data(), s, size ); }

	p[size] = 0;
	return p;
}

static Mutex kbdIntMutex; //NO lock in callback
static FSCInfo* volatile kbdIntInfo = 0;
static FSSftpParam* volatile kbdIntParam = 0;

void KbIntCallback(
   const char* /*name*/, int /*name_len*/,
   const char* /*instruction*/, int /*instruction_len*/,
   int num_prompts,
   const LIBSSH2_USERAUTH_KBDINT_PROMPT* prompts,
   LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses,
   void** /*anstract*/ )
{
	if ( num_prompts <= 0 ) { return; }

	if ( !kbdIntInfo ) { return; }

	try
	{

		std::vector<FSPromptData> pData( num_prompts );
		int i;

		for ( i = 0; i < num_prompts; i++ )
		{
			pData[i].visible = prompts[i].echo != 0;
			pData[i].prompt = utf8_to_unicode( CopyToStrZ( prompts[i].text, prompts[i].length ).data() ).data();
		}

		static unicode_t userSymbol[] = { '@', 0 };

		if ( !kbdIntInfo->Prompt(
		        utf8_to_unicode( "SFTP" ).data(),
		        carray_cat<unicode_t>( kbdIntParam->user.Data(), userSymbol, kbdIntParam->server.Data() ).data(),
		        pData.data(), num_prompts ) ) { return; }

		for ( i = 0; i < num_prompts; i++ )
		{
			std::vector<char> str = new_char_str( ( char* )FSString( pData[i].prompt.Data() ).Get( kbdIntParam->charset ) );

			if ( str.data() )
			{
				int l = strlen( str.data() );
				responses[i].length = l;
				responses[i].text = ( char* ) malloc( l + 1 );

				if ( responses[i].text ) { strcpy( responses[i].text, str.data() ); }

			}
		}

	}
	catch ( cexception* ex )
	{
		fprintf( stderr, "exception in kbdint callback used with libssh2: %s\n", ex->message() );
		ex->destroy();
	}
	catch ( ... )
	{
		fprintf( stderr, "excention (...) in kbdint callback used with libssh2\n" );
	}
}

int FSSftp::CheckSession( int* err, FSCInfo* info )
{

	if ( sshSession ) { return 0; }

	try
	{

		unsigned ip;
		int e;

		if ( !GetHostIp( unicode_to_utf8( _operParam.server.Data() ).data(), &ip, &e ) )
		{
			throw int( e );
		}

		_sock.Create();
		_sock.Connect( ntohl( ip ), _operParam.port );

		sshSession = libssh2_session_init();

		if ( !sshSession ) { throw int( SSH_INTERROR_X3 ); }

		libssh2_session_set_blocking( sshSession, 0 );

		WHILE_EAGAIN_( e, libssh2_session_handshake( sshSession, _sock.Id() ) );

		if ( e ) { throw int( e - 1000 ); }

		FSString userName = "";

		if ( _operParam.user.Data()[0] )
		{
			userName = _operParam.user.Data();
		}
		else
		{
#ifndef _WIN32
			char* ret = getenv( "LOGNAME" );

			if ( ret )
			{
				userName = FSString( sys_charset_id, ret );
				_operParam.user = userName.GetUnicode();

				MutexLock infoLock( &infoMutex );
				_infoParam.user = userName.GetUnicode();
			}

#endif
		};

		char* authList = 0;

		char* charUserName = ( char* )userName.Get( _operParam.charset );

		while ( true )
		{
			authList = libssh2_userauth_list( sshSession, charUserName, strlen( charUserName ) );

			if ( authList ) { break; }

			CheckSessionEagain();
			WaitSocket( info );
		}


		//publickey,password,keyboard-interactive
		static const char passId[] = "password";
		static const char kInterId[] = "keyboard-interactive";


		static unicode_t userSymbol[] = { '@', 0 };

		while ( true )
		{
			if ( !strncmp( authList, passId, strlen( passId ) ) )
			{
				FSPromptData data;
				data.visible = false;
				data.prompt = utf8_to_unicode( "Password:" ).data();

				if ( !info->Prompt(
				        utf8_to_unicode( "SFTP_" ).data(),
				        carray_cat<unicode_t>( userName.GetUnicode(), userSymbol, _operParam.server.Data() ).data(),
				        &data, 1 ) ) { throw int( SSH_INTERROR_STOPPED ); }

				int ret;
				WHILE_EAGAIN_( ret, libssh2_userauth_password( sshSession,
				                                               ( char* )FSString( _operParam.user.Data() ).Get( _operParam.charset ),
				                                               ( char* )FSString( data.prompt.Data() ).Get( _operParam.charset ) ) );

				if ( ret ) { throw int( ret - 1000 ); }

				break; //!!!
			}
			else if ( !strncmp( authList, kInterId, strlen( kInterId ) ) )
			{
				MutexLock lock( &kbdIntMutex );
				kbdIntInfo = info;
				kbdIntParam = &_operParam;

				int ret;
				WHILE_EAGAIN_( ret,
				               libssh2_userauth_keyboard_interactive( sshSession,
				                                                      ( char* )FSString( _operParam.user.Data() ).Get( _operParam.charset ),
				                                                      KbIntCallback )
				             );

				if ( ret ) { throw int( ret - 1000 ); }

				break; //!!!
			}

			char* s = authList;

			while ( *s && *s != ',' ) { s++; }

			if ( !*s ) { break; }

			authList = s + 1;
		};


		while ( true )
		{
			sftpSession = libssh2_sftp_init( sshSession );

			if ( sftpSession ) { break; }

			if ( !sftpSession )
			{
				int e = libssh2_session_last_errno( sshSession );

				if ( e != LIBSSH2_ERROR_EAGAIN ) { throw int( e - 1000 ); }
			}

			WaitSocket( info );
		}

		return 0;

	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		//if (sftpSession) ??? похоже закрытие сессии все решает
		if ( sshSession ) { libssh2_session_free( sshSession ); }

		sshSession = 0;
		sftpSession = 0;
		_sock.Close( false );
		return ( e == -2 ) ? -2 : -1;
	}

}

void FSSftp::CloseSession()
{
	//if (sftpSession) ??? похоже закрытие сессии все решает
	if ( sshSession ) { libssh2_session_free( sshSession ); }

	sshSession = 0;
	sftpSession = 0;

	if ( _sock.isValid() ) { _sock.Close( false ); }
}


unsigned FSSftp::Flags() { return HAVE_READ | HAVE_WRITE | HAVE_SYMLINK | HAVE_SEEK; }

bool  FSSftp::IsEEXIST( int err ) { return err == EEXIST; }
bool  FSSftp::IsENOENT( int err ) { return err == ENOENT; }
bool  FSSftp::IsEXDEV( int err ) { return err == EXDEV; }

bool FSSftp::Equal( FS* fs )
{
	if ( !fs || fs->Type() != FS::SFTP ) { return false; }

	if ( fs == this ) { return true; }

	FSSftp* f = ( FSSftp* )fs;

	MutexLock l1( &infoMutex );
	MutexLock l2( &( f->infoMutex ) );

	if ( _infoParam.isSet != f->_infoParam.isSet )
	{
		return false;
	}

	return !CmpStr<const unicode_t>( _infoParam.server.Data(), f->_infoParam.server.Data() ) &&
	       !CmpStr<const unicode_t>( _infoParam.user.Data(), f->_infoParam.user.Data() ) &&
	       _infoParam.port == f->_infoParam.port &&
	       _infoParam.charset == f->_infoParam.charset;
}



FSString FSSftp::StrError( int err )
{
	const char* s = "";

	if ( err < -500 )
	{
		switch ( err + 1000 )
		{
			case LIBSSH2_ERROR_SOCKET_NONE:
				s = "LIBSSH2_ERROR_SOCKET_NONE";
				break;

			case LIBSSH2_ERROR_BANNER_RECV:
				s = "LIBSSH2_ERROR_BANNER_RECV";
				break;

			case LIBSSH2_ERROR_BANNER_SEND:
				s = "LIBSSH2_ERROR_BANNER_SEND";
				break;

			case LIBSSH2_ERROR_INVALID_MAC:
				s = "LIBSSH2_ERROR_INVALID_MAC";
				break;

			case LIBSSH2_ERROR_KEX_FAILURE:
				s = "LIBSSH2_ERROR_KEX_FAILURE";
				break;

			case LIBSSH2_ERROR_ALLOC:
				s = "LIBSSH2_ERROR_ALLOC";
				break;

			case LIBSSH2_ERROR_SOCKET_SEND:
				s = "LIBSSH2_ERROR_SOCKET_SEND";
				break;

			case LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE:
				s = "LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE";
				break;

			case LIBSSH2_ERROR_TIMEOUT:
				s = "LIBSSH2_ERROR_TIMEOUT";
				break;

			case LIBSSH2_ERROR_HOSTKEY_INIT:
				s = "LIBSSH2_ERROR_HOSTKEY_INIT";
				break;

			case LIBSSH2_ERROR_HOSTKEY_SIGN:
				s = "LIBSSH2_ERROR_HOSTKEY_SIGN";
				break;

			case LIBSSH2_ERROR_DECRYPT:
				s = "LIBSSH2_ERROR_DECRYPT";
				break;

			case LIBSSH2_ERROR_SOCKET_DISCONNECT:
				s = "LIBSSH2_ERROR_SOCKET_DISCONNECT";
				break;

			case LIBSSH2_ERROR_PROTO:
				s = "LIBSSH2_ERROR_PROTO";
				break;

			case LIBSSH2_ERROR_PASSWORD_EXPIRED:
				s = "LIBSSH2_ERROR_PASSWORD_EXPIRED";
				break;

			case LIBSSH2_ERROR_FILE:
				s = "LIBSSH2_ERROR_FILE";
				break;

			case LIBSSH2_ERROR_METHOD_NONE:
				s = "LIBSSH2_ERROR_METHOD_NONE";
				break;

			case LIBSSH2_ERROR_AUTHENTICATION_FAILED:
				s = "Authentication failed";
				break;

			case LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED:
				s = "LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED";
				break;

			case LIBSSH2_ERROR_CHANNEL_OUTOFORDER:
				s = "LIBSSH2_ERROR_CHANNEL_OUTOFORDER";
				break;

			case LIBSSH2_ERROR_CHANNEL_FAILURE:
				s = "LIBSSH2_ERROR_CHANNEL_FAILURE";
				break;

			case LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED:
				s = "LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED";
				break;

			case LIBSSH2_ERROR_CHANNEL_UNKNOWN:
				s = "LIBSSH2_ERROR_CHANNEL_UNKNOWN";
				break;

			case LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED:
				s = "LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED";
				break;

			case LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED:
				s = "LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED";
				break;

			case LIBSSH2_ERROR_CHANNEL_CLOSED:
				s = "LIBSSH2_ERROR_CHANNEL_CLOSED";
				break;

			case LIBSSH2_ERROR_CHANNEL_EOF_SENT:
				s = "LIBSSH2_ERROR_CHANNEL_EOF_SENT";
				break;

			case LIBSSH2_ERROR_SCP_PROTOCOL:
				s = "LIBSSH2_ERROR_SCP_PROTOCOL";
				break;

			case LIBSSH2_ERROR_ZLIB:
				s = "LIBSSH2_ERROR_ZLIB";
				break;

			case LIBSSH2_ERROR_SOCKET_TIMEOUT:
				s = "LIBSSH2_ERROR_SOCKET_TIMEOUT";
				break;

			case LIBSSH2_ERROR_SFTP_PROTOCOL:
				s = "LIBSSH2_ERROR_SFTP_PROTOCOL";
				break;

			case LIBSSH2_ERROR_REQUEST_DENIED:
				s = "LIBSSH2_ERROR_REQUEST_DENIED";
				break;

			case LIBSSH2_ERROR_METHOD_NOT_SUPPORTED:
				s = "LIBSSH2_ERROR_METHOD_NOT_SUPPORTED";
				break;

			case LIBSSH2_ERROR_INVAL:
				s = "LIBSSH2_ERROR_INVAL";
				break;

			case LIBSSH2_ERROR_INVALID_POLL_TYPE:
				s = "LIBSSH2_ERROR_INVALID_POLL_TYPE";
				break;

			case LIBSSH2_ERROR_PUBLICKEY_PROTOCOL:
				s = "LIBSSH2_ERROR_PUBLICKEY_PROTOCOL";
				break;

			case LIBSSH2_ERROR_EAGAIN:
				s = "LIBSSH2_ERROR_EAGAIN";
				break;

			case LIBSSH2_ERROR_BUFFER_TOO_SMALL:
				s = "LIBSSH2_ERROR_BUFFER_TOO_SMALL";
				break;

			case LIBSSH2_ERROR_BAD_USE:
				s = "LIBSSH2_ERROR_BAD_USE";
				break;

			case LIBSSH2_ERROR_COMPRESS:
				s = "LIBSSH2_ERROR_COMPRESS";
				break;

			case LIBSSH2_ERROR_OUT_OF_BOUNDARY:
				s = "LIBSSH2_ERROR_OUT_OF_BOUNDARY";
				break;

			case LIBSSH2_ERROR_AGENT_PROTOCOL:
				s = "LIBSSH2_ERROR_AGENT_PROTOCOL";
				break;

			case LIBSSH2_ERROR_SOCKET_RECV:
				s = "LIBSSH2_ERROR_SOCKET_RECV";
				break;

			case LIBSSH2_ERROR_ENCRYPT:
				s = "LIBSSH2_ERROR_ENCRYPT";
				break;

			case LIBSSH2_ERROR_BAD_SOCKET:
				s = "LIBSSH2_ERROR_BAD_SOCKET";
				break;

			default:
				s = "LIBSSH2_ERROR_???";
				break;
		}

	}
	else
	{
		switch ( err )
		{
			case SSH_INTERROR_NOTSUPPORT:
				s = "not supported operation";
				break;

			case SSH_INTERROR_X3:
				s = "X3";
				break;

			case SSH_INTERROR_CONNECT:
				s = "connection failed";
				break;

			case SSH_INTERROR_AUTH:
				s = "authorization failed";
				break;

			case SSH_INTERROR_FATAL:
				s = "fatal ssh error";
				break;

			case SSH_INTERROR_STOPPED:
				s = "Process stopped";
				break;

			default:
				if ( err >= 0 )
				{
					sys_char_t buf[0x100];
					FSString str;
					str.SetSys( sys_error_str( err, buf, 0x100 ) );
					return str;
				}

				char buf[0x100];
				snprintf( buf, sizeof( buf ), "err : %i ???", err );
				FSString str( CS_UTF8, buf );
				return str;
		}
	}

	return FSString( CS_UTF8, s );
}

int FSSftp::OpenRead ( FSPath& path, int /*flags*/, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	int n = 0;

	for ( ; n < MAX_FILES; n++ )
		if ( !fileTable[n] ) { break; }

	if ( n >= MAX_FILES )
	{
		if ( err ) { *err = SSH_INTERROR_OUTOF; }

		return -1;
	}

	try
	{
		LIBSSH2_SFTP_HANDLE* fd = 0;

		while ( true )
		{
			fd = libssh2_sftp_open( sftpSession, ( char* )path.GetString( _operParam.charset, '/' ),
			                        LIBSSH2_FXF_READ,
			                        0 );

			if ( fd ) { break; }

			CheckSFTPEagain();
			WaitSocket( info );
		}

		fileTable[n] = fd;

	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return n;
}

int FSSftp::OpenCreate  ( FSPath& path, bool overwrite, int mode, int /*flags*/, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	int n = 0;

	try
	{
		if ( !overwrite )
		{
			/* странная херня, при наличии файла с  O_EXCL,  выдает не EEXIST, а "прерван системный вызов"
			   поэтому встанил эту дурацкую проверку на наличие
			*/
			LIBSSH2_SFTP_ATTRIBUTES attr;
			int ret;
			WHILE_EAGAIN_( ret, libssh2_sftp_lstat( sftpSession, ( char* )path.GetString( _operParam.charset, '/' ), &attr ) );

			if ( !ret ) { if ( err ) { *err = EEXIST; } return -1; }
		}

		for ( n = 0; n < MAX_FILES; n++ )
			if ( !fileTable[n] ) { break; }

		if ( n >= MAX_FILES )
		{
			if ( err ) { *err = SSH_INTERROR_OUTOF; }

			return -1;
		}


		LIBSSH2_SFTP_HANDLE* fd = 0;

		while ( true )
		{
			fd = libssh2_sftp_open( sftpSession,
			                        ( char* )path.GetString( _operParam.charset, '/' ),
			                        LIBSSH2_FXF_CREAT | LIBSSH2_FXF_WRITE | ( overwrite ? LIBSSH2_FXF_TRUNC : LIBSSH2_FXF_EXCL ),
			                        mode );

			if ( fd ) { break; }

			CheckSFTPEagain();
			WaitSocket( info );
		}

		fileTable[n] = fd;

	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return n;
}

int FSSftp::Close ( int fd, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( fd < 0 || fd >= MAX_FILES || !fileTable[fd] )
	{
		if ( err ) { *err = EINVAL; }

		return -1;
	}

	try
	{
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_close( fileTable[fd] ) );
		CheckSFTP( ret );

	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	fileTable[fd] = 0;
	return 0;
}

int FSSftp::Read  ( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( fd < 0 || fd >= MAX_FILES || !fileTable[fd] )
	{
		if ( err ) { *err = EINVAL; }

		return -1;
	}

	try
	{
		int bytes;
		WHILE_EAGAIN_( bytes, libssh2_sftp_read( fileTable[fd], ( char* )buf, size ) );

		if ( bytes < 0 ) { CheckSFTP( bytes ); }

		return bytes;
	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}
}


int FSSftp::Write ( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( fd < 0 || fd >= MAX_FILES || !fileTable[fd] )
	{
		if ( err ) { *err = EINVAL; }

		return -1;
	}

	try
	{
		int bytes = 0;
		char* s = ( char* )buf;

		while ( size > 0 )
		{
			int ret;
			WHILE_EAGAIN_( ret, libssh2_sftp_write( fileTable[fd], s, size ) );

			if ( ret < 0 ) { CheckSFTP( ret ); }

			if ( !ret ) { break; }

			bytes += ret;
			size -= ret;
			s += ret;
		}

		return bytes;
	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}
}

int FSSftp::Seek( int fd, SEEK_FILE_MODE mode, seek_t pos, seek_t* pRet,  int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( fd < 0 || fd >= MAX_FILES || !fileTable[fd] )
	{
		if ( err ) { *err = EINVAL; }

		return -1;
	}

	//???
	if ( mode == FSEEK_BEGIN )
	{
		libssh2_sftp_seek64( fileTable[fd],   pos );

		if ( pRet ) { *pRet = pos; }

		return 0;
	}

	if ( err )
	{
		*err = EINVAL;
	}

	return -1;
}


int FSSftp::Rename   ( FSPath&  oldpath, FSPath& newpath, int* err,  FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	try
	{
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_rename( sftpSession, ( char* ) oldpath.GetString( _operParam.charset, '/' ), ( char* ) newpath.GetString( _operParam.charset, '/' ) ) );
		CheckSFTP( ret );
	}
	catch ( int e )
	{

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}


int FSSftp::MkDir ( FSPath& path, int mode, int* err,  FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	try
	{
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_mkdir( sftpSession, ( char* )path.GetString( _operParam.charset, '/' ), mode ) );
		CheckSFTP( ret );
	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}

int FSSftp::Delete   ( FSPath& path, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	try
	{
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_unlink( sftpSession, ( char* )path.GetString( _operParam.charset, '/' ) ) );
		CheckSFTP( ret );
	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}

int FSSftp::RmDir ( FSPath& path, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	try
	{
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_rmdir( sftpSession, ( char* )path.GetString( _operParam.charset, '/' ) ) );
		CheckSFTP( ret );
	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}

int FSSftp::SetFileTime ( FSPath& path, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	LIBSSH2_SFTP_ATTRIBUTES attr;
	attr.flags = LIBSSH2_SFTP_ATTR_ACMODTIME;
	attr.atime = aTime;
	attr.mtime = mTime;

	try
	{
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_setstat( sftpSession, ( char* )path.GetString( _operParam.charset, '/' ), &attr ) );
		CheckSFTP( ret );
	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}

void FSSftp::CloseHandle( LIBSSH2_SFTP_HANDLE* h, FSCInfo* info )
{
	int ret;
	WHILE_EAGAIN_( ret, libssh2_sftp_close_handle( h ) );

	if ( ret ) { throw int( ret - 1000 ); }
}

struct SftpAttr
{
	LIBSSH2_SFTP_ATTRIBUTES attr;
	int Permissions() { return ( attr.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS ) ? attr.permissions : 0; }
	long long Size()  { return ( attr.flags & LIBSSH2_SFTP_ATTR_SIZE ) ? attr.filesize : 0; }
	int Uid()      { return ( attr.flags & LIBSSH2_SFTP_ATTR_UIDGID ) ? attr.uid : 0; }
	int Gid()      { return ( attr.flags & LIBSSH2_SFTP_ATTR_UIDGID ) ? attr.gid : 0; }
	time_t MTime()    { return ( attr.flags & LIBSSH2_SFTP_ATTR_ACMODTIME ) ? attr.mtime : 0; }
	bool IsLink()     { return ( attr.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS ) != 0 && ( attr.permissions & S_IFMT ) == S_IFLNK; }
};


int FSSftp::ReadDir  ( FSList* list, FSPath& path, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( !list ) { return 0; }

	list->Clear();

	try
	{

		LIBSSH2_SFTP_HANDLE* dir = 0;

		try
		{

			while ( true )
			{
				dir = libssh2_sftp_opendir( sftpSession, ( char* )path.GetString( _operParam.charset, '/' ) );

				if ( dir ) { break; }

				CheckSFTPEagain();
				WaitSocket( info );
			}

			while ( true )
			{
				char buf[4096];
				int len = 0;
				SftpAttr attr;


				WHILE_EAGAIN_( len, libssh2_sftp_readdir( dir, buf, sizeof( buf ) - 1, &attr.attr ) );

				if ( len < 0 ) { CheckSFTP( len ); }

				if ( len == 0 ) { break; }

				if ( buf[0] == '.' && ( !buf[1] || ( buf[1] == '.' && !buf[2] ) ) )
				{
					continue;
				}

				clPtr<FSNode> pNode = new FSNode();
				pNode->name.Set( _operParam.charset, buf );

				if ( attr.IsLink() )
				{
					FSPath pt = path;
					pt.Push( _operParam.charset, buf );
					char* fullPath = ( char* )pt.GetString( _operParam.charset, '/' );

					WHILE_EAGAIN_( len, libssh2_sftp_readlink( sftpSession, fullPath, buf, sizeof( buf ) - 1 ) );

					if ( len < 0 ) { CheckSFTP( len ); }

					pNode->st.link.Set( _operParam.charset, buf );

					int ret;
					WHILE_EAGAIN_( ret, libssh2_sftp_stat( sftpSession, fullPath, &attr.attr ) );
				}

				pNode->st.mode = attr.Permissions();
				pNode->st.size = attr.Size();
				pNode->st.uid = attr.Uid();
				pNode->st.gid = attr.Gid();
				pNode->st.mtime = attr.MTime();

				list->Append( pNode );
			}

		}
		catch ( ... )
		{
			if ( dir ) { CloseHandle( dir, info ); }

			throw;
		}

		if ( dir ) { CloseHandle( dir, info ); }

	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}


int FSSftp::Stat  ( FSPath& path, FSStat* st, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	char* fullPath = ( char* ) path.GetString( _operParam.charset, '/' );

	try
	{
		SftpAttr attr;
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_lstat( sftpSession, fullPath, &attr.attr ) );
		CheckSFTP( ret );

		if ( attr.IsLink() )
		{
			char buf[4096];
			int len;

			WHILE_EAGAIN_( len,  libssh2_sftp_readlink( sftpSession, fullPath, buf, sizeof( buf ) ) );

			if ( len < 0 ) { CheckSFTP( len ); };

			st->link.Set( _operParam.charset, buf );

			int ret;

			WHILE_EAGAIN_( ret, libssh2_sftp_stat( sftpSession, fullPath, &attr.attr ) );

			if ( ret ) { attr.attr.permissions = 0; }
		}

		st->mode  = attr.Permissions();
		st->size  = attr.Size();
		st->uid   = attr.Uid();
		st->gid   = attr.Gid();
		st->mtime = attr.MTime();
	}
	catch ( int e )
	{
		st->mode = 0;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}

int FSSftp::FStat ( int fd, FSStat* st, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( fd < 0 || fd >= MAX_FILES || !fileTable[fd] )
	{
		if ( err ) { *err = EINVAL; }

		return -1;
	}

	try
	{
		SftpAttr attr;
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_fstat( fileTable[fd], &attr.attr ) );
		CheckSFTP( ret );

		st->mode  = attr.Permissions();
		st->size  = attr.Size();
		st->uid   = attr.Uid();
		st->gid   = attr.Gid();
		st->mtime = attr.MTime();
	}
	catch ( int e )
	{
		st->mode = 0;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;


}

int FSSftp::Symlink  ( FSPath& path, FSString& str, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	try
	{
		int ret;
		WHILE_EAGAIN_( ret, libssh2_sftp_symlink( sftpSession, ( char* )str.Get( _operParam.charset ),  ( char* )path.GetString( _operParam.charset, '/' ) ) );
		CheckSFTP( ret );
	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}


FSString FSSftp::Uri( FSPath& path )
{
	MutexLock lock( &infoMutex ); //infoMutex!!!

	std::vector<char> a;

	char port[0x100];
	snprintf( port, sizeof( port ), ":%i", _infoParam.port );

	FSString server( _infoParam.server.Data() );

	FSString user( _infoParam.user.Data() );
	a = carray_cat<char>( "sftp://", user.GetUtf8(), "@",  server.GetUtf8(), port, path.GetUtf8( '/' ) );

	return FSString( CS_UTF8, a.data() );
}



FSSftp::~FSSftp()
{
	CloseSession();
}

#endif

