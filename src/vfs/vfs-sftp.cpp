/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#ifdef LIBSSH_EXIST

#include "vfs-sftp.h"


#include "operthread.h" //for carray_cat, needs to be reworked

#include <libssh/callbacks.h>

static int ssh_wal_mutex_init ( void** priv )
{
	*priv = malloc ( sizeof ( wal::mutex_t ) );

	if ( !*priv ) { return ENOMEM; }

	int err = wal::mutex_create( ( wal::mutex_t* ) *priv );

	if ( err )
	{
		free( *priv );
		*priv = 0;
	}

	return err;
}

static int ssh_wal_mutex_destroy ( void** lock )
{
	int err = wal::mutex_delete( ( wal::mutex_t* )*lock );
	free ( *lock );
	*lock = NULL;
	return err;
}

static int ssh_wal_mutex_lock ( void** lock )  {  return wal::mutex_lock ( ( wal::mutex_t* )*lock ); }
static int ssh_wal_mutex_unlock ( void** lock )   {  return wal::mutex_unlock ( ( wal::mutex_t* )*lock ); }
static unsigned long ssh_wal_thread_id ( void )   {  return ( unsigned long ) wal::thread_self(); }

static struct ssh_threads_callbacks_struct ssh_threads_wal;

void InitSSH()
{
	ssh_threads_wal.type = "threads_wal";
	ssh_threads_wal.mutex_init = ssh_wal_mutex_init;
	ssh_threads_wal.mutex_destroy = ssh_wal_mutex_destroy;
	ssh_threads_wal.mutex_lock = ssh_wal_mutex_lock;
	ssh_threads_wal.mutex_unlock  = ssh_wal_mutex_unlock;
	ssh_threads_wal.thread_id  = ssh_wal_thread_id;
	ssh_threads_set_callbacks( &ssh_threads_wal );

	ssh_init();
}



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


int FSSftp::CheckSession( int* err, FSCInfo* info )
{
	if ( sshSession ) { return 0; }

	try
	{
		sshSession = ssh_new();

		if ( !sshSession ) { throw int( SSH_INTERROR_X3 ); }

		if ( ssh_options_set( sshSession, SSH_OPTIONS_HOST, unicode_to_utf8( _operParam.server.Data() ).ptr() ) )
		{
			throw int( SSH_INTERROR_X3 );
		}

		int port = _operParam.port;

		if ( ssh_options_set( sshSession, SSH_OPTIONS_PORT, &port ) )
		{
			throw int( SSH_INTERROR_X3 );
		}


		FSString userName = "";

		if ( _operParam.user.Data()[0] )
		{
			userName = _operParam.user.Data();
		}
		else
		{
			char* ret = getenv( "LOGNAME" );

			if ( ret )
			{
				userName = FSString( sys_charset_id, ret );
				_operParam.user = userName.GetUnicode();

				MutexLock infoLock( &infoMutex );
				_infoParam.user = userName.GetUnicode();
			}
		};

		if ( ssh_options_set( sshSession, SSH_OPTIONS_USER, ( char* )userName.Get( _operParam.charset ) ) ) //not sure if utf8 is needed
		{
			throw int( SSH_INTERROR_X3 );
		}

		if ( ssh_connect( sshSession ) != SSH_OK )
		{
			throw int( SSH_INTERROR_CONNECT );
		}


		int method = ssh_userauth_list( sshSession, 0 );

		int ret;


		static unicode_t* userSymbol = "@";

		if ( method & SSH_AUTH_METHOD_PASSWORD )
		{
			FSPromptData data;
			data.visible = false;
			data.prompt = utf8_to_unicode( "Password:" ).ptr();



			if ( !info->Prompt(
			        utf8_to_unicode( "SFTP_" ).ptr(),
			        carray_cat<unicode_t>( userName.GetUnicode(), userSymbol, _operParam.server.Data() ).ptr(),
			        &data, 1 ) ) { throw int( SSH_INTERROR_STOPPED ); }

			ret = ssh_userauth_password( sshSession,
			                             ( char* )FSString( _operParam.user.Data() ).Get( _operParam.charset ),
			                             ( char* )FSString( data.prompt.Data() ).Get( _operParam.charset ) );
		}

		if ( ret != SSH_AUTH_SUCCESS &&
		     ( method & SSH_AUTH_METHOD_INTERACTIVE ) != 0 )
		{
			while ( true )
			{
				ret = ssh_userauth_kbdint( sshSession, 0, 0 );

				if ( ret != SSH_AUTH_INFO ) { break; }

				const char* instruction = ssh_userauth_kbdint_getinstruction( sshSession );

				if ( !instruction ) { instruction = ""; }

				int n = ssh_userauth_kbdint_getnprompts( sshSession );

				if ( n <= 0 ) { continue; }

				std::vector<FSPromptData> pData( n );
				int i;

				for ( i = 0; i < n; i++ )
				{
					char echo;
					const char* prompt = ssh_userauth_kbdint_getprompt( sshSession, i, &echo );

					if ( !prompt ) { break; }

					pData[i].visible = echo != 0;
					pData[i].prompt = utf8_to_unicode( prompt ).ptr();
				}

				if ( !info ) { throw int( SSH_INTERROR_AUTH ); }

				if ( !info->Prompt(
				        utf8_to_unicode( "SFTP" ).ptr(),
				        carray_cat<unicode_t>( userName.GetUnicode(), userSymbol, _operParam.server.Data() ).ptr(),
				        pData.ptr(), n ) ) { throw int( SSH_INTERROR_STOPPED ); }

				for ( i = 0; i < n; i++ )
				{
					if ( ssh_userauth_kbdint_setanswer( sshSession, i, ( char* )FSString( pData[i].prompt.Data() ).Get( _operParam.charset ) ) < 0 )
					{
						throw int( SSH_INTERROR_AUTH );
					}
				}
			}
		}

		if ( ret != SSH_AUTH_SUCCESS )
		{
			if ( ret == SSH_AUTH_PARTIAL )
			{
				throw int( SSH_INTERROR_UNSUPPORTED_AUTH );
			}
			else
			{
				throw int( SSH_INTERROR_AUTH );
			}
		}

		sftpSession = sftp_new( sshSession );

		if ( !sftpSession ) { throw int( SSH_INTERROR_FATAL ); }

		if ( sftp_init( sftpSession ) != SSH_OK ) { throw int( SSH_INTERROR_FATAL ); }

	}
	catch ( int e )
	{
		if ( err ) { *err = e; }

		if ( sftpSession ) { sftp_free( sftpSession ); }

		if ( sshSession ) { ssh_free( sshSession ); }

		sshSession = 0;
		sftpSession = 0;
		return ( e == SSH_INTERROR_STOPPED ) ? -2 : -1;
	}

	return 0;
}

void FSSftp::CloseSession()
{
	if ( sftpSession )
	{
		sftp_free( sftpSession );
		sftpSession = 0;
	}

	if ( sshSession )
	{
		ssh_disconnect( sshSession );
		ssh_free( sshSession );
		sshSession = 0;
	}
}


unsigned FSSftp::Flags() { return HAVE_READ | HAVE_WRITE | HAVE_SYMLINK; }
bool  FSSftp::IsEEXIST( int err ) { return err == SSH_FX_FILE_ALREADY_EXISTS; }
bool  FSSftp::IsENOENT( int err ) { return err == SSH_FX_NO_SUCH_FILE; }
bool  FSSftp::IsEXDEV( int err ) { return false; /*err == EXDEV;*/ }


FSString FSSftp::StrError( int err )
{
	const char* s = "";

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

		case SSH_FX_OK:
			s = "No error";
			break;

		case SSH_FX_EOF:
			s = "End-of-file encountered";
			break;

		case SSH_FX_NO_SUCH_FILE:
			s = "File doesn't exist";
			break;

		case SSH_FX_PERMISSION_DENIED:
			s = "Permission denied";
			break;

		case SSH_FX_FAILURE:
			s = "Generic failure";
			break;

		case SSH_FX_BAD_MESSAGE:
			s = "Garbage received from server";
			break;

		case SSH_FX_NO_CONNECTION:
			s = "No connection has been set up";
			break;

		case SSH_FX_CONNECTION_LOST:
			s = "There was a connection, but we lost it";
			break;

		case SSH_FX_OP_UNSUPPORTED:
			s = "Operation not supported by the server";
			break;

		case SSH_FX_INVALID_HANDLE:
			s = "Invalid file handle";
			break;

		case SSH_FX_NO_SUCH_PATH:
			s = "No such file or directory path exists";
			break;

		case SSH_FX_FILE_ALREADY_EXISTS:
			s = "An attempt to create an already existing file or directory has been made";
			break;

		case SSH_FX_WRITE_PROTECT:
			s = "We are trying to write on a write-protected filesystem";
			break;

		case SSH_FX_NO_MEDIA:
			s = "No media in remote drive";
			break;


		/* case SSH_NO_ERROR: s = "no error"; break;
		   case SSH_REQUEST_DENIED: s = "recoverable error"; break;
		   case SSH_FATAL: s = "fatal ssh error"; break;
		   case SSH_INTERROR_OUTOF: s = "out of invernal sftp vfs resources"; break;
		*/
		default:
			s = "???";
			break;
	}

	return FSString( CS_UTF8, s );
}

int FSSftp::OpenRead ( FSPath& path, int* err, FSCInfo* info )
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

	sftp_file f = sftp_open( sftpSession, ( char* ) path.GetString( _operParam.charset, '/' ), O_RDONLY, 0 );

	if ( !f )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	fileTable[n] = f;

	return n;
}

int FSSftp::OpenCreate  ( FSPath& path, bool overwrite, int mode, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( !overwrite )
	{
		/*
		   it was strange that sftp_open with O_EXCL returns "generc error" when files exists, instead of EEXIST
			then added this stupid check for existence
		*/
		sftp_attributes a = sftp_lstat( sftpSession, ( char* ) path.GetString( _operParam.charset, '/' ) );

		if ( a )
		{
			sftp_attributes_free( a ); //!!!

			if ( err ) { *err = SSH_FX_FILE_ALREADY_EXISTS; }

			return -1;
		}
	}


	int n = 0;

	for ( ; n < MAX_FILES; n++ )
		if ( !fileTable[n] ) { break; }

	if ( n >= MAX_FILES )
	{
		if ( err ) { *err = SSH_INTERROR_OUTOF; }

		return -1;
	}

	sftp_file f = sftp_open( sftpSession, ( char* ) path.GetString( _operParam.charset, '/' ),
	                         O_CREAT | O_WRONLY | ( overwrite ? O_TRUNC : O_EXCL ),
	                         mode );

	if ( !f )
	{
//printf("ssh-err:'%s'\n",ssh_get_error(sshSession));
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	fileTable[n] = f;

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

	ret = sftp_close( fileTable[fd] );
	fileTable[fd] = 0;

	if ( ret != SSH_NO_ERROR )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	return 0;
}

int FSSftp::Read  ( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );

	int ret = CheckSession( err, info ); //???

	if ( ret ) { return ret; }

	if ( fd < 0 || fd >= MAX_FILES || !fileTable[fd] )
	{
		if ( err ) { *err = EINVAL; }

		return -1;
	}



	/*
	   just in case as in Write
	*/
	int bytes = 0;
	char* s = ( char* )buf;

	while ( size > 0 )
	{
		int n = size;

		if ( n > 0x4000 )
		{
			n = 0x4000;
		}

		int ret = sftp_read( fileTable[fd], s, n );

		if ( ret < 0 )
		{
			if ( err ) { *err = sftp_get_error( sftpSession ); }

			return -1;
		}

		if ( !ret ) { break; }

		s += ret;
		size -= ret;
		bytes += ret;
	}

	return bytes;

	/*
	int bytes = sftp_read(fileTable[fd], buf, size);


	if (bytes<0)
	{
	   if (err) *err = sftp_get_error(sftpSession);
	   return -1;
	}
	return bytes;
	*/
}


int FSSftp::Write ( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );

	int ret = CheckSession( err, info ); //???

	if ( ret ) { return ret; }

	if ( fd < 0 || fd >= MAX_FILES || !fileTable[fd] )
	{
		if ( err ) { *err = EINVAL; }

		return -1;
	}


	/*
	   libssh corrupts data when size is 65536, but if 16k then OK, so have to decrease
		but reads such blocks successfully
		(needs to be decreased there also, just in case)

	   this is how it was
	   int bytes = sftp_write(fileTable[fd], buf, size);
	*/

	int bytes = 0;
	char* s = ( char* )buf;

	while ( size > 0 )
	{
		int n = size;

		if ( n > 0x4000 )
		{
			n = 0x4000;
		}

		int ret = sftp_write( fileTable[fd], s, n );

		if ( ret < 0 )
		{
			if ( err ) { *err = sftp_get_error( sftpSession ); }

			return -1;
		}

		if ( !ret ) { break; }

		s += ret;
		size -= ret;
		bytes += ret;
	}

	return bytes;
}


int FSSftp::Rename   ( FSPath&  oldpath, FSPath& newpath, int* err,  FSCInfo* info )
{
	MutexLock lock( &mutex );

	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }


	if ( sftp_rename( sftpSession, ( char* ) oldpath.GetString( _operParam.charset, '/' ), ( char* ) newpath.GetString( _operParam.charset, '/' ) ) )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	return 0;
}


int FSSftp::MkDir ( FSPath& path, int mode, int* err,  FSCInfo* info )
{
	MutexLock lock( &mutex );

	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( sftp_mkdir( sftpSession, ( char* )path.GetString( _operParam.charset ), mode ) )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	return 0;
}

int FSSftp::Delete   ( FSPath& path, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );

	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( sftp_unlink( sftpSession, ( char* )path.GetString( _operParam.charset ) ) )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	return 0;
}

int FSSftp::RmDir ( FSPath& path, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );

	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( sftp_rmdir( sftpSession, ( char* )path.GetString( _operParam.charset ) ) )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	return 0;
}

int FSSftp::SetFileTime ( FSPath& path, FSTime cTime, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );

	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	struct timeval tv[2];

	tv[0].tv_sec  = aTime;

	tv[0].tv_usec = 0;

	tv[1].tv_sec  = mTime;

	tv[1].tv_usec = 0;

	if ( sftp_utimes( sftpSession, ( char* )path.GetString( _operParam.charset ), tv ) )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	return 0;
}

int FSSftp::ReadDir  ( FSList* list, FSPath& path, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( !list ) { return 0; }

	list->Clear();

	sftp_dir dir = sftp_opendir( sftpSession, ( char* )path.GetString( _operParam.charset ) );

	if ( !dir )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	try
	{

		while ( true )
		{
			if ( info && info->Stopped() )
			{
				sftp_closedir( dir );
				return -2;
			}

			sftp_attributes attr = sftp_readdir( sftpSession, dir );

			if ( !attr )
			{
				if ( sftp_dir_eof( dir ) ) { break; }

				if ( err ) { *err = sftp_get_error( sftpSession ); }

				return -1;
			}

			try
			{
				//skip . and ..
				if ( !attr->name || attr->name[0] == '.' && ( !attr->name[1] || ( attr->name[1] == '.' && !attr->name[2] ) ) )
				{
					continue;
				}

				clPtr<FSNode> pNode = new FSNode();

#if defined(__APPLE__)
				if ( _operParam.charset == CS_UTF8 )
				{
					std::string normname = normalize_utf8_NFC( attr->name );
					pNode->name.Set( _operParam.charset, normname.data() );
				}
				else
				{
					pNode->name.Set( _operParam.charset, attr->name );
				}
#else
				pNode->name.Set( _operParam.charset, attr->name );
#endif
				pNode->st.size = attr->size;
				pNode->st.uid = attr->uid;
				pNode->st.gid = attr->gid;
				pNode->st.mtime = attr->mtime;

				if ( attr->type == SSH_FILEXFER_TYPE_SYMLINK )
				{
					FSPath pt = path;
					pt.Push( _operParam.charset, attr->name );
					char* fullPath = ( char* )pt.GetString( _operParam.charset );
					char* s = sftp_readlink( sftpSession, fullPath );

					if ( s ) { pNode->st.link.Set( _operParam.charset, s ); }

					sftp_attributes a = sftp_stat( sftpSession, fullPath );

					if ( a )
					{
						pNode->st.mode  = a->permissions;
						pNode->st.mtime = a->mtime;
						sftp_attributes_free( a );
					}
					else
					{
						pNode->st.mode = 0;
					}
				}
				else
				{
					pNode->st.mode = attr->permissions;
				}

				list->Append( pNode );

				sftp_attributes_free( attr );

			}
			catch ( ... )
			{
				sftp_attributes_free( attr );
				throw;
			}
		};

		sftp_closedir( dir );
	}
	catch ( ... )
	{
		sftp_closedir( dir );
		throw;
	}

	return 0;
}


int FSSftp::Stat  ( FSPath& path, FSStat* st, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	char* fullPath = ( char* ) path.GetString( _operParam.charset );

	sftp_attributes la = sftp_lstat( sftpSession, fullPath );

	if ( !la )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	if ( la->type == SSH_FILEXFER_TYPE_SYMLINK )
	{
		char* s = sftp_readlink( sftpSession, fullPath );

		if ( s ) { st->link.Set( _operParam.charset, s ); }

		sftp_attributes_free( la ); //!!!
	}
	else
	{
		st->mode  = la->permissions;
		st->size  = la->size;
		st->uid   = la->uid;
		st->gid   = la->gid;
		st->mtime = la->mtime;

		sftp_attributes_free( la ); //!!!

		return 0;
	}

	sftp_attributes a = sftp_stat( sftpSession, fullPath );

	if ( !a )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	st->mode  = la->permissions;
	st->size  = la->size;
	st->uid   = la->uid;
	st->gid   = la->gid;
	st->mtime = la->mtime;

	sftp_attributes_free( a ); //!!!

	return 0;
}

int FSSftp::Symlink  ( FSPath& path, FSString& str, int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );
	int ret = CheckSession( err, info );

	if ( ret ) { return ret; }

	if ( sftp_symlink( sftpSession, ( char* )str.Get( _operParam.charset ),  ( char* )path.GetString( _operParam.charset ) ) )
	{
		if ( err ) { *err = sftp_get_error( sftpSession ); }

		return -1;
	}

	return 0;

}


FSString FSSftp::Uri( FSPath& path )
{
	MutexLock lock( &infoMutex ); //infoMutex!!!

	std::string a;

	char port[0x100];
	snprintf( port, sizeof( port ), ":%i", _infoParam.port );

	FSString server( _infoParam.server.Data() );

	FSString user( _infoParam.user.Data() );
	a = std::string( "sftp://" ) + user.GetUtf8() + "@" + server.GetUtf8() + std::string(port) + path.GetUtf8();

	return FSString( CS_UTF8, a.ptr() );
}



FSSftp::~FSSftp()
{
	CloseSession();
}

#endif

