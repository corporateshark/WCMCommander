/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "vfs-smb.h"

#ifdef LIBSMBCLIENT_EXIST

#include "string-util.h"

#ifdef LIBSMB40
#  include <samba-4.0/libsmbclient.h>
#else
#  include <libsmbclient.h>
#endif

#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>

#ifdef __linux__
#  define OPENFLAG_LARGEFILE (O_LARGEFILE)
#else
#  define OPENFLAG_LARGEFILE (0)
#endif

static Mutex smbMutex;
static SMBCCTX* smbCTX = 0;
static FSCInfo* fscInfo = 0;
static FSSmbParam* currentFsParam = 0;
static FSSmbParam lastFsParam;

#define FREPARE_SMB_OPER(lockname, infoname, param)   MutexLock lockname(&smbMutex); fscInfo = infoname; currentFsParam = param;


struct PathBuffer
{
	std::vector<char> p;
	int size;
	int minPos;
	int pos;

	PathBuffer();
	void Clear() { pos = minPos; p[pos] = 0; }

	void Cut( const char* s );
	char* Set( const char* path );
	char* SetPath( FSPath& path ) {return Set( ( char* ) path.GetString( CS_UTF8, '/' ) ); }
};


PathBuffer::PathBuffer()
	:  p( 16 ), size( 16 ), pos( 0 )
{
	strcpy( p.data(), "smb://" );
	minPos = pos = strlen( p.data() );
}

void PathBuffer::Cut( const char* s )
{
	int l = strlen( s );
	int nsize = pos + l + 1;

	if ( nsize > size )
	{
		nsize = ( ( nsize + 0x100 - 1 ) / 0x100 ) * 0x100;
		std::vector<char> t( nsize );

		if ( pos > 0 ) { memcpy( t.data(), p.data(), pos ); }

		t[pos] = 0;
		p = t;
		size = nsize;
	}

	memcpy( p.data() + pos, s, l + 1 );
	pos += l;
}

char* PathBuffer::Set( const char* path )
{
	Clear();

	if ( !currentFsParam ) { return p.data(); }

	if ( currentFsParam->server[0] )
	{
		if ( currentFsParam->user[0] )
		{
			Cut( const_cast<char*>( currentFsParam->user ) );
			Cut( "@" );
		}

		Cut( const_cast<char*>( currentFsParam->server ) );
		Cut( "/" );
	}

	if ( path )
	{
		if ( path[0] == '/' ) { path++; }

		Cut( path );
	}

	return p.data();
}

static PathBuffer pathBuffer1;
static PathBuffer pathBuffer2;

static void SetString( char* dest, int len, const char* src )
{
	for ( ; *src && len > 1; dest++, src++, len-- ) { *dest = *src; }

	if ( len > 0 ) { *dest = 0; }
}

//static int authIteration = 0; // 0 - search in cache >0 ask user
//static bool authCancelled;

static void smbcAuth( const char* srv, const char* shr,  char* wg, int wglen, char* un, int unlen, char* pw, int pwlen )
{
	if ( !currentFsParam->server[0] ) //ходим по сети
	{
		return;
	}

	//printf("Auth! %s %s %s(%i) %s(%i)\n", srv, shr, wg, wglen, un, unlen);
	//printf("currentFsParam->user = '%s'\n", currentFsParam->user);

	if ( !currentFsParam->isSet )
	{
		FSSmbParam param =   ( !currentFsParam->user[0] || !strcmp( const_cast<char*>( currentFsParam->user ), const_cast<char*>( lastFsParam.user ) ) ) &&
		                     ( !currentFsParam->domain[0] || !strcmp( const_cast<char*>( currentFsParam->domain ), const_cast<char*>( lastFsParam.domain ) ) )
		                     ? lastFsParam : *currentFsParam;
		strcpy( const_cast<char*>( param.server ), const_cast<char*>( currentFsParam->server ) );

		if ( !param.user[0] && unlen > 0 ) { SetString( const_cast<char*>( param.user ), sizeof( param.user ), un ); }

		if ( !param.domain[0] && wglen > 0 ) { SetString( const_cast<char*>( param.domain ), sizeof( param.domain ), wg ); }

		if ( fscInfo && fscInfo->SmbLogon( &param ) )
		{
			lastFsParam = *currentFsParam = param;
		}
		else
		{
			//...
			return;
		}
	}

	if ( currentFsParam->user[0] )
	{
		SetString( wg, wglen, const_cast<char*>( currentFsParam->domain ) );
		SetString( un, unlen, const_cast<char*>( currentFsParam->user ) );
		SetString( pw, pwlen, const_cast<char*>( currentFsParam->pass ) );
	}

	return;
}

static void InitSmb()
{
	MutexLock lock( &smbMutex );

	if ( smbCTX ) { return; }

	smbCTX = smbc_new_context();

	if ( !smbCTX ) { throw_syserr( 0, "smbclient can't allocate context" ); }

	if ( !smbc_init_context( smbCTX ) )
	{
		smbc_free_context( smbCTX, 0 );
		smbCTX = 0;
		throw_syserr( 0, "smbclient can't init context" );
	}

	smbc_set_context( smbCTX );
	smbc_setFunctionAuthData( smbCTX, smbcAuth );
	smbc_setOptionUrlEncodeReaddirEntries( smbCTX, 0 );
}

FSSmb::FSSmb( FSSmbParam* param )
	:  FS( SAMBA )
{
	InitSmb();

	if ( param ) { _param = *param; }
}

unsigned FSSmb::Flags() { return HAVE_READ | HAVE_WRITE | HAVE_SEEK; };
bool  FSSmb::IsEEXIST( int err ) { return err == EEXIST; }
bool  FSSmb::IsENOENT( int err ) { return err == ENOENT; }
bool  FSSmb::IsEXDEV( int err ) { return err == EXDEV; }

FSString FSSmb::StrError( int err )
{
	sys_char_t buf[1024];
	FSString ret( sys_charset_id, ( char* )sys_error_str( err, buf, sizeof( buf ) ) );
	return ret;
}

bool FSSmb::Equal( FS* fs )
{
	if ( !fs || fs->Type() != FS::SAMBA ) { return false; }

	return true;
}

int FSSmb::OpenRead( FSPath& path, int flags, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_open( pathBuffer1.SetPath( path ), O_RDONLY | OPENFLAG_LARGEFILE, 0 );
	SetError( err, errno );
	return n < 0 ? -1 : n;
}

int FSSmb::OpenCreate( FSPath& path, bool overwrite, int mode, int flags, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_open( pathBuffer1.SetPath( path ),
	                   O_CREAT | O_WRONLY | O_TRUNC | OPENFLAG_LARGEFILE | ( overwrite ? 0 : O_EXCL ) , mode );
	SetError( err, errno );
	return n < 0 ? -1 : n;
}

int FSSmb::Close( int fd, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	if ( smbc_close( fd ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}

int FSSmb::Read( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_read( fd, buf, size );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	return n;
}

int FSSmb::Write( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_write( fd, buf, size );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	return n;
}

int FSSmb::Seek( int fd, SEEK_FILE_MODE mode, seek_t pos, seek_t* pRet,  int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int whence = 0;

	switch ( mode )
	{
		case FSEEK_POS:
			whence = SEEK_CUR;
			break;

		case FSEEK_END:
			whence   = SEEK_END;
			break;

		case FSEEK_BEGIN:
			whence = SEEK_SET;
			break;

		default:
			whence   = SEEK_SET;
	};

	seek_t n = smbc_lseek( fd, pos, whence );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	if ( pRet ) { *pRet = n; }

	return 0;
}


int FSSmb::Rename( FSPath&  oldpath, FSPath& newpath, int* err,  FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_rename(
	           pathBuffer1.SetPath( oldpath ),
	           pathBuffer2.SetPath( newpath )
	        );
	SetError( err, errno );
	return n < 0 ? -1 : n;
}

int FSSmb::MkDir( FSPath& path, int mode, int* err,  FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_mkdir( pathBuffer1.SetPath( path ), mode );
	SetError( err, errno );
	return n < 0 ? -1 : n;
}

int FSSmb::Delete( FSPath& path, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_unlink( pathBuffer1.SetPath( path ) );
	SetError( err, errno );
	return n < 0 ? -1 : n;
}

int FSSmb::RmDir( FSPath& path, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	int n = smbc_rmdir( pathBuffer1.SetPath( path ) );
	SetError( err, errno );
	return n < 0 ? -1 : n;
}


int FSSmb::SetFileTime( FSPath& path, FSTime cTime, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	struct timeval tv[2];
	tv[0].tv_sec  = aTime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec  = mTime;
	tv[1].tv_usec = 0;
	int n = smbc_utimes( pathBuffer1.SetPath( path ), tv );
	SetError( err, errno );
	return n < 0 ? -1 : n;
}


struct big_stat
{
	struct stat st;
	char buf[0x100];
};

static int SMB_STAT( const char* url, struct stat* st )
{
	big_stat s;
	int r = smbc_stat( url, &s.st );
	*st = s.st;
	return r;
}


static int InternalStat( FSPath& path, FSStat* fsStat, FSCInfo* info )
{
	struct stat st;

	if ( SMB_STAT( pathBuffer1.SetPath( path ), &st ) )
	{
		return -1;
	}

	fsStat->mode   = ( st.st_mode & ~( S_IXUSR | S_IXGRP | S_IXOTH ) );
	fsStat->size   = st.st_size;
	fsStat->m_CreationTime   = st.st_ctime;
	fsStat->m_LastAccessTime = st.st_atime;
	fsStat->m_LastWriteTime  = st.st_mtime;
	fsStat->m_ChangeTime = st.st_mtime;
	fsStat->gid = st.st_gid;
	fsStat->uid = st.st_uid;
	fsStat->dev = st.st_dev;
	fsStat->ino = st.st_ino;
	return 0;
}


int FSSmb::Stat( FSPath& path, FSStat* fsStat, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );
	ASSERT( fsStat );

	if ( InternalStat( path, fsStat, info ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}

static int SMB_FSTAT( int fd, struct stat* st )
{
	big_stat s;
	int r = smbc_fstat( fd, &s.st );
	*st = s.st;
	return r;
}


static int InternalFStat( int fd, FSStat* fsStat, FSCInfo* info )
{
	struct stat st;

	if ( SMB_FSTAT( fd, &st ) )
	{
		return -1;
	}

	fsStat->mode   = ( st.st_mode & ~( S_IXUSR | S_IXGRP | S_IXOTH ) );
	fsStat->size   = st.st_size;
	fsStat->m_CreationTime   = st.st_ctime;
	fsStat->m_LastAccessTime = st.st_atime;
	fsStat->m_LastWriteTime  = st.st_mtime;
	fsStat->m_ChangeTime = st.st_mtime;
	fsStat->gid = st.st_gid;
	fsStat->uid = st.st_uid;
	fsStat->dev = st.st_dev;
	fsStat->ino = st.st_ino;
	return 0;
}


int FSSmb::FStat( int fd, FSStat* st, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );
	ASSERT( st );

	if ( InternalFStat( fd, st, info ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}


int FSSmb::Symlink( FSPath& path, FSString& str, int* err, FSCInfo* info ) //EPERM
{
	SetError( err, EPERM );
	return -1;
}

int FSSmb::StatVfs( FSPath& path, FSStatVfs* vst, int* err, FSCInfo* info )
{

	SetError( err, 0 );
	return -1;
	/*

	FREPARE_SMB_OPER(lock, info, &_param);
	ASSERT(vst);

	struct statvfs st;
	memset(&st, 0, sizeof(st));

	if (smbc_statvfs(pathBuffer1.SetPath(path), &st))
	{
	   SetError(err, errno);
	   return -1;
	}

	////////////// works, but when first called returns "no talloc stackframe around, leaking memory"
	if (!st.f_frsize) st.f_frsize = st.f_bsize;

	vst->size = int64_t(st.f_blocks) * st.f_frsize;
	vst->avail = int64_t(st.f_bavail) * st.f_bsize;

	return 0;
	*/
}

FSString FSSmb::Uri( FSPath& path )
{
	MutexLock lock( &mutex );
	std::string a;

	if ( _param.server[0] )
	{
		if ( _param.user[0] )
		{
			a = std::string( "smb://" ) + std::string( (char*)_param.user ) + "@" + std::string( (char*)_param.server ) + path.GetUtf8();
		}
		else
		{
			a = std::string( "smb://" ) + std::string( (char*)_param.server ) + path.GetUtf8();
		}
	}
	else
	{
		a = std::string( "smb:/" ) + path.GetUtf8();
	}

	return FSString( CS_UTF8, a.c_str() );
}

int FSSmb::ReadDir( FSList* list, FSPath& _path, int* err, FSCInfo* info )
{
	FREPARE_SMB_OPER( lock, info, &_param );

	if ( info && info->Stopped() ) { return -2; }

	list->Clear();

	FSPath path( _path );

	int d = smbc_opendir( pathBuffer1.SetPath( path ) );

	if ( d < 0 )
	{
		SetError( err, errno );
		return -1;
	}

	try
	{
		struct smbc_dirent* pEnt;

		int n = path.Count();

		while ( true )
		{
			if ( info && info->Stopped() )
			{
				smbc_closedir( d );
				return -2;
			}

			pEnt = smbc_readdir( d );

			if ( !pEnt )
			{
				//???
				break;
			}

			//skip . and ..
			if ( pEnt->name[0] == '.' && ( !pEnt->name[1] || ( pEnt->name[1] == '.' && !pEnt->name[2] ) ) )
			{
				continue;
			}


			if ( //ignore it
			   pEnt->smbc_type == SMBC_PRINTER_SHARE ||
			   pEnt->smbc_type == SMBC_IPC_SHARE
			) { continue; }


			clPtr<FSNode> pNode = new FSNode();
			path.SetItem( n, CS_UTF8, pEnt->name );

			switch ( pEnt->smbc_type )
			{
				case  SMBC_WORKGROUP:
					pNode->extType = FSNode::WORKGROUP;
					pNode->st.mode = S_IFDIR;
					break;

				case  SMBC_SERVER:
					pNode->extType = FSNode::SERVER;
					pNode->st.mode = S_IFDIR;
					break;

				case  SMBC_FILE_SHARE:
					pNode->extType = FSNode::FILESHARE;
					pNode->st.mode = S_IFDIR;
					break;

				case  SMBC_PRINTER_SHARE:
				case  SMBC_COMMS_SHARE:
				case  SMBC_IPC_SHARE:
				case  SMBC_DIR:
					pNode->st.mode = S_IFDIR;
					break;

				default:
					InternalStat( path, &pNode->st, info );
			}

#if defined(__APPLE__)
			if ( sys_charset_id == CS_UTF8 )
			{
				std::string normname = normalize_utf8_NFC( pEnt->name );
				pNode->name.Set( sys_charset_id, normname.data() );
			}
			else
			{
				pNode->name.Set( sys_charset_id, pEnt->name );
			}
#else
			pNode->name.Set( sys_charset_id, pEnt->name );
#endif

			list->Append( pNode );
		};

		smbc_closedir( d );

		return 0;
	}
	catch ( ... )
	{
		smbc_closedir( d );
		throw;
	}
}

FSSmb::~FSSmb()
{
}

#endif //LIBSMBCLIENT_EXIST
