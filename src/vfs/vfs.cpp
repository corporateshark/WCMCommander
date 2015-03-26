/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include "vfs.h"
#include "string-util.h"
#include <sys/types.h>
#include "unicode_lc.h"
#include <algorithm>

#if defined( __APPLE__ )
#  include <sys/param.h>
#  include <sys/mount.h>
#endif

#include <unordered_map>

///////////////////////////////////////////////////  FS /////////////////////////////
int FS::OpenRead  ( FSPath& path, int flags, int* err, FSCInfo* info ) { SetError( err, 0 ); return -1; }
int FS::OpenCreate   ( FSPath& path, bool overwrite, int mode, int flags, int* err, FSCInfo* info ) { SetError( err, 0 ); return -1; }
int FS::Close  ( int fd, int* err, FSCInfo* info )            { SetError( err, 0 ); return -1; }
int FS::Read   ( int fd, void* buf, int size, int* err, FSCInfo* info )      { SetError( err, 0 ); return -1; }
int FS::Write  ( int fd, void* buf, int size, int* err, FSCInfo* info )      { SetError( err, 0 ); return -1; }
int FS::Seek   ( int fd, SEEK_FILE_MODE mode, seek_t pos, seek_t* pRet,  int* err, FSCInfo* info )   { SetError( err, 0 ); return -1; }
int FS::Rename ( FSPath&  oldpath, FSPath& newpath, int* err, FSCInfo* info )   { SetError( err, 0 ); return -1; }
int FS::MkDir  ( FSPath& path, int mode, int* err,  FSCInfo* info )    { SetError( err, 0 ); return -1; }
int FS::Delete ( FSPath& path, int* err, FSCInfo* info )            { SetError( err, 0 ); return -1; }
int FS::RmDir  ( FSPath& path, int* err, FSCInfo* info )            { SetError( err, 0 ); return -1; }
int FS::SetFileTime  ( FSPath& path, FSTime cTime, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )  { SetError( err, 0 ); return -1; }
int FS::ReadDir   ( FSList* list, FSPath& path,  int* err, FSCInfo* info )    { SetError( err, 0 ); return -1; }
int FS::Stat( FSPath& path, FSStat* st, int* err, FSCInfo* info )         { SetError( err, 0 ); return -1; }
int FS::StatSetAttr( FSPath& path, const FSStat* st, int* err, FSCInfo* info ) { SetError( err, 0 ); return -1; }
int FS::FStat( int fd, FSStat* st, int* err, FSCInfo* info )     { SetError( err, 0 ); return -1; }
int FS::Symlink   ( FSPath& path, FSString& str, int* err, FSCInfo* info )      { SetError( err, 0 ); return -1; }
int FS::StatVfs( FSPath& path, FSStatVfs* st, int* err, FSCInfo* info )    { SetError( err, 0 ); return -1; }
int64_t FS::GetFileSystemFreeSpace( FSPath& path, int* err ) { SetError( err, 0 ); return -1; }

unicode_t* FS::GetUserName( int user, unicode_t buf[64] ) { buf[0] = 0; return buf; };
unicode_t* FS::GetGroupName( int group, unicode_t buf[64] ) { buf[0] = 0; return buf; };

FS::~FS() {}

////////////////////////////////////// FSCInfo
bool FSCInfo::SmbLogon( FSSmbParam* a ) { return false; }
bool FSCInfo::FtpLogon( FSFtpParam* a ) { return false; }
bool FSCInfo::Stopped() { return false; }
bool FSCInfo::Prompt( const unicode_t* header, const unicode_t* message, FSPromptData* p, int count ) { return false; }
FSCInfo::~FSCInfo() {}

////////////////////////////////////// FSCSimpleInfo
bool FSCSimpleInfo::Stopped() {   MutexLock lock( &mutex ); return stopped; }
FSCSimpleInfo::~FSCSimpleInfo() {}

//////////////////////////////////////////////////  FSSys ///////////////////////////


#ifdef _WIN32
//#error
#include <time.h>


static void TT_to_FILETIME( time_t t, FILETIME& ft )
{
	LONGLONG ll;
	ll = Int32x32To64( t, 10000000 ) + 116444736000000000ll;
	ft.dwLowDateTime = ( DWORD )(ll & 0xFFFFFFFF );
	ft.dwHighDateTime = ( DWORD )(( ll >> 32 ) & 0xFFFFFFFF );
}

static time_t FILETIME_to_TT( FILETIME ft )
{
	LONGLONG ll =  ft.dwHighDateTime;
	ll <<= 32;
	ll += ft.dwHighDateTime;
	ll -= 116444736000000000ll;

	if ( ll < 0 ) { return 0; }

	ll /= 10000000;
	return ll;
}

time_t FSTime::GetTimeT()
{
	if ( flags & TIME_T_OK ) { return tt; }

	if ( ( flags & FILETIME_OK ) == 0 ) { return 0; }

	tt = FILETIME_to_TT( ft );
	flags |= TIME_T_OK;
	return tt;
}

FILETIME FSTime::GetFileTime()
{
	if ( flags & FILETIME_OK ) { return ft; }

	static FILETIME t0 = {0, 0};

	if ( ( flags & TIME_T_OK ) == 0 ) { return t0; }

	TT_to_FILETIME( tt, ft );
	flags |= FILETIME_OK;
	return ft;
}

inline int Utf16Chars( const wchar_t* s ) { int n = 0; for ( ; *s; s++ ) { n++; } return n; }

inline std::vector<wchar_t> UnicodeToUtf16_cat( const unicode_t* s, wchar_t* cat )
{
	int lcat = Utf16Chars( cat );
	std::vector<wchar_t> p( unicode_strlen( s ) + lcat + 1 );
	wchar_t* d;

	for ( d = p.data(); *s; s++, d++ ) { *d = *s; }

	for ( ; *cat; cat++, d++ ) { *d = *cat; }

	*d = 0;
	return p;
}



unsigned FSSys::Flags() { return HAVE_READ | HAVE_WRITE | HAVE_SYMLINK | HAVE_SEEK; }
bool  FSSys::IsEEXIST( int err ) { return err == ERROR_FILE_EXISTS; } //err == EEXIST; }
bool  FSSys::IsENOENT( int err ) { return err == ERROR_FILE_NOT_FOUND ; } // err == ENOENT; }
bool  FSSys::IsEXDEV( int err ) { return false; } // err == EXDEV; }

FSString FSSys::StrError( int err )
{
	sys_char_t buf[1024];
	FSString ret;
	ret.SetSys( sys_error_str( err, buf, 0x100 ) );
	return ret;
}

bool FSSys::Equal( FS* fs )
{
	if ( !fs || fs->Type() != FS::SYSTEM 
#ifdef _WIN32		
		|| _drive != ((FSSys*)fs)->_drive
#endif		
		) { return false; }

	return true;
}

static std::vector<wchar_t> SysPathStr( int drive, const unicode_t* s )
{
	std::vector<wchar_t> p( 10 + unicode_strlen( s ) + 1 ); //+10 just in case, really +8 (max \\?\UNC\)
	wchar_t* d = p.data();

	d[0] = '\\';
	d[1] = '\\';
	d[2] = '?';
	d[3] = '\\';
	d += 4;

	if ( drive == -1 ) //???
	{
		d[0] = 'U';
		d[1] = 'N';
		d[2] = 'C';
		//d[3]='\\'; //plus \ will be added from the path
		d += 3;
	}
	else
	{
		d[0] = drive + 'a';
		d[1] = ':';
		d += 2;
	}

	for ( ; *s; s++, d++ ) { *d = *s; }

	*d = 0;
	return p;
}


int FSSys::OpenRead  ( FSPath& path, int flags, int* err, FSCInfo* info )
{
	int shareFlags = 0;

	if ( flags & FS::SHARE_READ ) { shareFlags |= FILE_SHARE_READ; }

	if ( flags & FS::SHARE_WRITE ) { shareFlags |= FILE_SHARE_WRITE; }

	HANDLE h = CreateFileW( SysPathStr( _drive, path.GetUnicode( '\\' ) ).data(), GENERIC_READ, shareFlags, 0, OPEN_EXISTING, 0, 0 );

	//file_open(SysPathStr(_drive, path.GetUnicode('\\')).ptr());
	if ( h == INVALID_HANDLE_VALUE )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	int fd = handles.New();
	HANDLE* p = this->handles.Handle( fd );

	if ( !p )
	{
		SetError( err, ERROR_INVALID_PARAMETER );
		CloseHandle( h );
		return -1;
	}

	*p = h;
	return fd;
}


int FSSys::OpenCreate   ( FSPath& path, bool overwrite, int mode, int flags,  int* err, FSCInfo* info )
{
	DWORD diseredAccess = GENERIC_READ | GENERIC_WRITE;
//	DWORD shareMode = 0;
	DWORD creationDisposition = ( overwrite ) ? CREATE_ALWAYS  : CREATE_NEW;
//???

	HANDLE h = CreateFileW( SysPathStr( _drive, path.GetUnicode( '\\' ) ).data(), diseredAccess, FILE_SHARE_WRITE, 0, creationDisposition, 0, 0 );

	if ( h == INVALID_HANDLE_VALUE )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	int fd = handles.New();
	HANDLE* p = this->handles.Handle( fd );

	if ( !p )
	{
		SetError( err, ERROR_INVALID_PARAMETER );
		CloseHandle( h );
		return -1;
	}

	*p = h;
	return fd;
}

int FSSys::Close( int fd, int* err, FSCInfo* info )
{
	HANDLE* p = this->handles.Handle( fd );

	if ( !p ) { SetError( err, ERROR_INVALID_PARAMETER ); return -1; }

	HANDLE h = *p;
	handles.Free( fd );

	if ( !CloseHandle( h ) )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	return 0;
}

int FSSys::Read( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	HANDLE* p = this->handles.Handle( fd );

	if ( !p ) { SetError( err, ERROR_INVALID_PARAMETER ); return -1; }

	DWORD bytes;

	if ( !ReadFile( *p, buf, size, &bytes, 0 ) )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	return bytes;
}

int FSSys::Write( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	HANDLE* p = this->handles.Handle( fd );

	if ( !p ) { SetError( err, ERROR_INVALID_PARAMETER ); return -1; }

	DWORD bytes;

	if ( !WriteFile( *p, buf, size, &bytes, 0 ) )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	return bytes;
}

int FSSys::Seek( int fd, SEEK_FILE_MODE mode, seek_t pos, seek_t* pRet,  int* err, FSCInfo* info )
{
	HANDLE* p = this->handles.Handle( fd );

	if ( !p ) { SetError( err, ERROR_INVALID_PARAMETER ); return -1; }

	LARGE_INTEGER li;
	li.QuadPart = pos;

	if ( !SetFilePointerEx( *p, li, &li, mode ) )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	if ( pRet )
	{
		*pRet = li.QuadPart;
	}

	return 0;
}

int FSSys::Rename ( FSPath&  oldpath, FSPath& newpath, int* err,  FSCInfo* info )
{
	if ( MoveFileW(
	        SysPathStr( _drive, oldpath.GetUnicode( '\\' ) ).data(),
	        SysPathStr( _drive, newpath.GetUnicode( '\\' ) ).data()
	     ) ) { return 0; }

	SetError( err, GetLastError() );
	return -1;
}

int FSSys::MkDir( FSPath& path, int mode, int* err,  FSCInfo* info )
{
	if ( CreateDirectoryW( SysPathStr( _drive, path.GetUnicode( '\\' ) ).data(), 0 ) ) { return 0; }

	DWORD e = GetLastError();

	if ( e == ERROR_ALREADY_EXISTS ) { return 0; }

	SetError( err, e );
	return -1;
}

int FSSys::Delete( FSPath& path, int* err, FSCInfo* info )
{
	std::vector<wchar_t> sp = SysPathStr( _drive, path.GetUnicode( '\\' ) );

	if ( DeleteFileW( sp.data() ) ) { return 0; }

	DWORD lastError  = GetLastError();

	if ( lastError == ERROR_ACCESS_DENIED ) //maybe read only attribute, try to reset
	{
		if ( SetFileAttributesW( sp.data(), 0 ) && DeleteFileW( sp.data() ) ) { return 0; }

		lastError  = GetLastError();
	}

	SetError( err, lastError );
	return -1;
}

int FSSys::RmDir( FSPath& path, int* err, FSCInfo* info )
{
	std::vector<wchar_t> sp = SysPathStr( _drive, path.GetUnicode( '\\' ) );

	if ( RemoveDirectoryW( sp.data() ) ) { return 0; }

	DWORD lastError  = GetLastError();

	if ( lastError == ERROR_ACCESS_DENIED ) //maybe read only attribute, try to reset
	{
		if ( SetFileAttributesW( sp.data(), 0 ) && RemoveDirectoryW( sp.data() ) ) { return 0; }

		lastError  = GetLastError();
	}

	SetError( err, lastError );
	return -1;
}

int FSSys::SetFileTime  ( FSPath& path, FSTime cTime, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )
{
	HANDLE h = CreateFileW( SysPathStr( _drive, path.GetUnicode( '\\' ) ).data(), FILE_WRITE_ATTRIBUTES , 0, 0, OPEN_EXISTING, 0, 0 );

	if ( h == INVALID_HANDLE_VALUE )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	FILETIME ct = cTime;
	FILETIME at = aTime;
	FILETIME mt = mTime;

	if ( !::SetFileTime( h, &ct, &at, &mt ) )
	{
		SetError( err, GetLastError() );
		CloseHandle( h );
		return -1;
	}

	CloseHandle( h );
	return 0;
}

// make UNC path by concati'ing input pars in an intelligent way
static std::vector<wchar_t> FindPathStr( int drive, const unicode_t* s, const wchar_t* cat )
{
	int lcat = Utf16Chars( cat );

	std::vector<wchar_t> p( 10 + unicode_strlen( s ) + lcat + 1 );
	wchar_t* d = p.data();

	d[0] = '\\';
	d[1] = '\\';
	d[2] = '?';
	d[3] = '\\';
	d += 4;

	if ( drive == -1 ) //???
	{
		d[0] = 'U';
		d[1] = 'N';
		d[2] = 'C';
		//d[3]='\\'; //plus \ will be added from the path
		d += 3;
	}
	else
	{
		d[0] = drive + 'a';
		d[1] = ':';
		d += 2;
	}


	unicode_t lastChar = 0;

	for ( ; *s; s++, d++ ) { lastChar = *d = *s; }

	// ensure that we do not append double-backslash to the filepath.
	// FindFirstFileW does not like UNC like \\?\c:\\*, and prefers \\?\c:\*
	if ( lastChar == '\\' && *cat == '\\' ) { cat++; }

	for ( ; *cat; cat++, d++ ) { *d = *cat; }

	*d = 0;

	return p;
}

#ifdef _DEBUG
static void toStr( char* str, const wchar_t* wstr )
{
	while ( *wstr )
	{
		*str++ = char( ( *wstr++ ) & 0xFF );
	}

	*str = 0;
}
#endif

int FSSys::ReadDir( FSList* list, FSPath& _path, int* err, FSCInfo* info )
{
	list->Clear();
	FSPath path( _path );
	WIN32_FIND_DATAW ent;

	HANDLE handle = FindFirstFileW( FindPathStr( _drive, path.GetUnicode(), L"\\*" ).data(), &ent );

#if defined( _DEBUG ) & 0
	std::vector<wchar_t> wpath = FindPathStr( _drive, path.GetUnicode(), L"\\*" );
	char s[1024];
	toStr( s, wpath.data() );
	dbg_printf( "FSSys::ReadDir %s@UNC path=%s\n", handle == INVALID_HANDLE_VALUE ? "OK" : "failed", s );
#endif

	if ( handle == INVALID_HANDLE_VALUE )
	{
		DWORD ret = GetLastError();

		if ( ret == ERROR_FILE_NOT_FOUND ) { return 0; }

		SetError( err, GetLastError() );
		return -1;
	}

	try
	{
		while ( true )
		{
			if ( info && info->Stopped() )
			{
				FindClose( handle );
				return -2;
			}

			//skip . and ..
			if ( !( ent.cFileName[0] == '.' && ( !ent.cFileName[1] || ( ent.cFileName[1] == '.' && !ent.cFileName[2] ) ) ) )
			{
				clPtr<FSNode> pNode = new FSNode();
				pNode->name.Set( CS_UNICODE, Utf16ToUnicode( ent.cFileName ).data() );

				pNode->st.dwFileAttributes = ent.dwFileAttributes;
				pNode->st.size = ( seek_t( ent.nFileSizeHigh ) << 32 ) + ent.nFileSizeLow;

				pNode->st.m_CreationTime = ent.ftCreationTime;
				pNode->st.m_LastAccessTime = ent.ftLastAccessTime;
				pNode->st.m_LastWriteTime = ent.ftLastWriteTime;
				// TODO: use ZwQueryDirectoryFile() to get the change time
				pNode->st.m_ChangeTime = ent.ftLastWriteTime;

				if ( ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
				{
					pNode->st.mode = S_IFDIR;
				}
				else
				{
					pNode->st.mode = S_IFREG;
				}

				pNode->st.mode |= 0664;
				list->Append( pNode );
			}

			if ( !FindNextFileW( handle, &ent ) )
			{
				if ( GetLastError() == ERROR_NO_MORE_FILES ) { break; }

				SetError( err, GetLastError() );
				FindClose( handle );
				return -1;
			}
		};

		FindClose( handle );

		return 0;
	}
	catch ( ... )
	{
		FindClose( handle );
		throw;
	}

	SetError( err, 100 );
	return -1;
}

int FSSys::Stat( FSPath& path, FSStat* fsStat, int* err, FSCInfo* info )
{
	if ( ( _drive >= 0 && path.Count() == 1 ) || ( _drive == -1 && path.Count() == 3 ) )
	{
		//pseudo stat
		fsStat->size = 0;
		fsStat->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		fsStat->mode = S_IFDIR;
		fsStat->m_CreationTime = 0;
		fsStat->m_LastAccessTime = 0;
		fsStat->m_LastWriteTime = 0;
		fsStat->m_ChangeTime = 0;
		fsStat->mode |= 0664;
		return 0;
	}

	WIN32_FIND_DATAW ent;
	HANDLE handle = FindFirstFileW( SysPathStr( _drive, path.GetUnicode() ).data(), &ent );

	if ( handle == INVALID_HANDLE_VALUE )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	try
	{
		fsStat->size = ( seek_t( ent.nFileSizeHigh ) << 32 ) + ent.nFileSizeLow;
		fsStat->dwFileAttributes = ent.dwFileAttributes;
		fsStat->m_CreationTime = ent.ftCreationTime;
		fsStat->m_LastAccessTime = ent.ftLastAccessTime;
		fsStat->m_LastWriteTime = ent.ftLastWriteTime;
		// TODO: use ZwQueryDirectoryFile() to get the change time
		fsStat->m_ChangeTime = ent.ftLastWriteTime;

		if ( ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{
			fsStat->mode = S_IFDIR;
		}
		else
		{
			fsStat->mode = S_IFREG;
		}

		fsStat->mode |= 0664;
		FindClose( handle );
		return 0;
	}
	catch ( ... )
	{
		FindClose( handle );
		throw;
	}

	//...
	SetError( err, 50 );
	return -1;
}

int FSSys::StatSetAttr( FSPath& path, const FSStat* st, int* err, FSCInfo* info )
{
	const unicode_t* lpFileName = path.GetUnicode();

	DWORD Attr = st->dwFileAttributes;

	return SetFileAttributesW( lpFileName, Attr ) ? 0 : -1;
}

int64_t FSSys::GetFileSystemFreeSpace( FSPath& path, int* err )
{
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;

	int d = Drive();

	char RootPath[] = { char( d + 'A' ), ':', '\\', 0 };

	if ( GetDiskFreeSpace( RootPath, &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters ) != TRUE ) { return -1; }

	return ( int64_t )SectorsPerCluster * ( int64_t )BytesPerSector * ( int64_t )NumberOfFreeClusters;
}

int FSSys::FStat( int fd, FSStat* fsStat, int* err, FSCInfo* info )
{
	BY_HANDLE_FILE_INFORMATION e;
	HANDLE* p = this->handles.Handle( fd );

	if ( !p ) { SetError( err, ERROR_INVALID_PARAMETER ); return -1; }


	if ( !GetFileInformationByHandle( *p, &e ) )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	fsStat->size = ( seek_t( e.nFileSizeHigh ) << 32 ) + e.nFileSizeLow;
	fsStat->dwFileAttributes = e.dwFileAttributes;
	fsStat->m_CreationTime = e.ftCreationTime;
	fsStat->m_LastAccessTime = e.ftLastAccessTime;
	fsStat->m_LastWriteTime = e.ftLastWriteTime;
	// TODO: use ZwQueryDirectoryFile() to get the change time
	fsStat->m_ChangeTime = e.ftLastWriteTime;

	if ( e.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
	{
		fsStat->mode = S_IFDIR;
	}
	else
	{
		fsStat->mode = S_IFREG;
	}

	fsStat->mode |= 0664;
	return 0;
}

int FSSys::Symlink( FSPath& path, FSString& str, int* err, FSCInfo* info )
{
	//...
	SetError( err, 50 );
	return -1;
}

int FSSys::StatVfs( FSPath& path, FSStatVfs* vst, int* err, FSCInfo* info )
{
	ccollect<wchar_t, 0x100> root;

	root.append( '\\' );
	root.append( '\\' );
	root.append( '?' );
	root.append( '\\' );

	if ( Drive() == -1 )
	{
		root.append( 'U' );
		root.append( 'N' );
		root.append( 'C' );
		root.append( '\\' );
		int n = path.Count() < 3 ? path.Count() : 3;

		for ( int i = 1; i < n; i++ )
		{
			const unicode_t* s = path.GetItem( i )->GetUnicode();

			for ( ; *s; s++ ) { root.append( *s ); }

			root.append( '\\' );
		}
	}
	else
	{
		root.append( Drive() + 'a' );
		root.append( ':' );
		root.append( '\\' );
	}

	root.append( 0 );

	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	DWORD NumberOfFreeClusters;
	DWORD TotalNumberOfClusters;

	if ( !GetDiskFreeSpaceW( root.ptr(), &SectorsPerCluster, &BytesPerSector, &NumberOfFreeClusters, &TotalNumberOfClusters ) )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	vst->size = int64_t( TotalNumberOfClusters ) * SectorsPerCluster * BytesPerSector;
	vst->avail = int64_t( NumberOfFreeClusters ) * SectorsPerCluster * BytesPerSector;
	return 0;
}

FSString FSSys::Uri( FSPath& path )
{
	unicode_t pref[0x100];
	unicode_t* p = pref;

	if ( _drive > 0 && _drive < 'z' - 'a' + 1 ) //+1 ???
	{
		*( p++ ) = 'A' + _drive;
		*( p++ ) = ':';
	}
	else if ( _drive == -1 )
	{
		*( p++ ) = '\\';
	}
	else
	{
		*( p++ ) = '?';
		*( p++ ) = ':';
	}

	*p = 0;

	return FSString( carray_cat<unicode_t>( pref, path.GetUnicode() ).data() );
}

static std::unordered_map<int, std::vector<unicode_t> > userList;
static std::unordered_map<int, std::vector<unicode_t> > groupList;

static unicode_t c0 = 0;

static unicode_t* GetOSUserName( int id )
{
	return &c0;
}

static unicode_t* GetOSGroupName( int id )
{
	return &c0;
}

unicode_t* FSSys::GetUserName( int user, unicode_t buf[64] )
{
	unicode_t* u = GetOSUserName( user );
	unicode_t* s = buf;

	for ( int n = 63; n > 0 && *u; n--, u++, s++ )
	{
		*s = *u;
	}

	*s = 0;
	return buf;
};


unicode_t* FSSys::GetGroupName( int group, unicode_t buf[64] )
{
	unicode_t* g = GetOSGroupName( group );
	unicode_t* s = buf;

	for ( int n = 63; n > 0 && *g; n--, g++, s++ )
	{
		*s = *g;
	}

	*s = 0;

	return buf;
};

//////////////////////////  W32NetRes  /////////////////////////

void W32NetRes::Set( NETRESOURCEW* p )
{
	Clear();

	if ( !p ) { return; }

	int nameLen = p->lpLocalName ? wcslen( p->lpLocalName ) + 1 : 0;
	int remoteNameLen = p->lpRemoteName ? wcslen( p->lpRemoteName ) + 1 : 0;
	int commentLen = p->lpComment ? wcslen( p->lpComment ) + 1 : 0;
	int providerLen = p->lpProvider ? wcslen( p->lpProvider ) + 1 : 0;
	int size = sizeof( Node ) + ( nameLen + remoteNameLen + commentLen + providerLen ) * sizeof( wchar_t );
	data = new unsigned char[size];
	Node& node = *( ( Node* )data );
	node.size = size;
	node.rs = *p;
	int offset = sizeof( node );
#if _MSC_VER > 1700
#  define QQQ(a, b, len) if (len>0) { a = (wchar_t*)(data + offset); Lwcsncpy(a, len, b, _TRUNCATE); } offset += len*sizeof(wchar_t);
#else
#  define QQQ(a, b, len) if (len>0) { a = (wchar_t*)(data + offset); Lwcsncpy(a, b, len); } offset += len*sizeof(wchar_t);
#endif
	QQQ( node.rs.lpLocalName, p->lpLocalName, nameLen );
	QQQ( node.rs.lpRemoteName, p->lpRemoteName, remoteNameLen );
	QQQ( node.rs.lpComment, p->lpComment, commentLen );
	QQQ( node.rs.lpProvider, p->lpProvider, providerLen );
#undef QQQ
}

unsigned char* W32NetRes::Copy( const unsigned char* a )
{
	if ( !a ) { return 0; }

	Node& src = *( ( Node* )a );

	unsigned char* b = new unsigned char [src.size];
	memcpy( b, a, src.size );

	Node& dest = *( ( Node* )b );
#define QQQ(x) if (dest.rs.x) { dest.rs.x = (wchar_t*)(( ((char*)src.rs.x)-((char*)a)) + (char*)b); }
	QQQ( lpLocalName );
	QQQ( lpRemoteName );
	QQQ( lpComment );
	QQQ( lpProvider );
#undef QQQ
	return b;
};


/////////////////////////// FSWin32Net /////////////////////////

unsigned FSWin32Net::Flags() { return 0; }
bool FSWin32Net::IsEEXIST( int err ) { return false; };
bool FSWin32Net::IsENOENT( int err ) { return false; };
bool FSWin32Net::IsEXDEV( int err ) { return false; };

FSString FSWin32Net::StrError( int err )
{
	if ( err == ERRNOSUPPORT )
	{
		return FSString( "Operation not supported by net fs" );
	}

	FSString ret;
	sys_char_t buf[1024];
	ret.SetSys( sys_error_str( err, buf, 0x100 ) );

	return ret;
}


bool FSWin32Net::Equal( FS* fs ) { return false; };

int FSWin32Net::OpenRead   ( FSPath& path, int flags, int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::OpenCreate ( FSPath& path, bool overwrite, int mode, int flags, int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::Close   ( int fd, int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::Read ( int fd, void* buf, int size, int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::Write   ( int fd, void* buf, int size, int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::Seek( int fd, SEEK_FILE_MODE mode, seek_t pos, seek_t* pRet,  int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::Rename  ( FSPath&  oldpath, FSPath& newpath, int* err,  FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::MkDir   ( FSPath& path, int mode, int* err,  FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::Delete  ( FSPath& path, int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }
int FSWin32Net::RmDir   ( FSPath& path, int* err, FSCInfo* info ) { SetError( err, ERRNOSUPPORT ); return -1; }

class WNetEnumerator
{
	enum { BUFSIZE = 16 * 1024 };
	HANDLE handle;
	std::vector<char> buf;
	int pos, count;
	bool Fill( DWORD* pErr );
public:
	WNetEnumerator(): handle( 0 ), buf( BUFSIZE ), pos( 0 ), count( 0 ) {}

	void Close() { if ( handle ) { WNetCloseEnum( handle ); handle = 0; pos = count = 0; } }
	bool Open( NETRESOURCEW* p ) { return WNetOpenEnumW( RESOURCE_GLOBALNET, RESOURCETYPE_ANY, 0, p, &handle ) == NO_ERROR; }

	NETRESOURCEW* Next( DWORD* pErr )
	{

		if ( !Fill( pErr ) ) { return 0; }

		if ( pErr ) { *pErr = 0; }

		if ( count < 0 ) { return 0; }

		NETRESOURCEW* p = ( ( NETRESOURCEW* )buf.data() ) + pos;
		pos++;
		return p;
	}

	~WNetEnumerator() { Close(); }
};

bool WNetEnumerator::Fill( DWORD* pErr )
{
	if ( count < 0 || pos < count ) { return true; }

	DWORD n = -1;
	DWORD bSize = BUFSIZE;
	DWORD res = WNetEnumResourceW( handle, &n, buf.data(), &bSize );

	if ( res == ERROR_NO_MORE_ITEMS )
	{
		count = -1;
		return true;
	}

	if ( res !=  NO_ERROR )
	{
		count = -1;

		if ( pErr ) { *pErr = GetLastError(); }

		return false;
	}

	count = n;
	pos = 0;
	return true;
}


int FSWin32Net::SetFileTime( FSPath& path, FSTime cTime, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )
{
	if ( err ) { *err = ERRNOSUPPORT; } return -1;
}

int FSWin32Net::ReadDir ( FSList* list, FSPath& path, int* err, FSCInfo* info )
{
	list->Clear();

	if ( path.Count() > 1 )
	{
		if ( err ) { *err = ERRNOSUPPORT; }

		return -1;
	}


	WNetEnumerator en;

	if ( !en.Open( _res.Get() ) )
	{
		SetError( err, GetLastError() );
		return -1;
	}

	try
	{
		while ( true )
		{
			if ( info && info->Stopped() )
			{
				return -2;
			}

			DWORD dwErr = 0;
			NETRESOURCEW* p = en.Next( &dwErr );

			if ( !p )
			{
				if ( !dwErr ) { break; }

				SetError( err, dwErr );
				return -1;
			}

			if ( !p->lpRemoteName ) { continue; }

			wchar_t* pName = p->lpRemoteName;

			if ( p->dwDisplayType == RESOURCEDISPLAYTYPE_SHARE || p->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER )
			{
				//remove server name from shared resource name and wrong simbols from server name
				wchar_t* last = 0;

				for ( wchar_t* s = pName; *s; s++ )
					if ( *s == '\\' ) { last = s; }

				if ( last && last[1] ) { pName = last + 1; }
			}

			clPtr<FSNode> pNode = new FSNode();

			pNode->name.Set( CS_UNICODE, Utf16ToUnicode( pName ).data() );
			pNode->st.mode = S_IFDIR;
			pNode->st.mode |= 0664;

			pNode->_w32NetRes = W32NetRes( p );

			switch ( p->dwDisplayType )
			{
				//case RESOURCEDISPLAYTYPE_GENERIC: return "GENERIC";
				case RESOURCEDISPLAYTYPE_DOMAIN:
				case RESOURCEDISPLAYTYPE_GROUP:
				case RESOURCEDISPLAYTYPE_NETWORK:
					pNode->extType = FSNode::WORKGROUP;
					break;

				case RESOURCEDISPLAYTYPE_SERVER:
					pNode->extType = FSNode::SERVER;
					break;

				case RESOURCEDISPLAYTYPE_DIRECTORY:
				case RESOURCEDISPLAYTYPE_SHARE:
					pNode->extType = FSNode::FILESHARE;
					break;
			};

			list->Append( pNode );
		};

		return 0;
	}
	catch ( ... )
	{
		throw;
	}

	SetError( err, 100 );
	return -1;
}

int FSWin32Net::Stat ( FSPath& path, FSStat* st, int* err, FSCInfo* info ) { if ( err ) { *err = ERRNOSUPPORT; } return -1; }
int FSWin32Net::Symlink ( FSPath& path, FSString& str, int* err, FSCInfo* info ) { if ( err ) { *err = ERRNOSUPPORT; } return -1; }

FSString FSWin32Net::Uri( FSPath& path )
{
	NETRESOURCEW* p = _res.Get();

	if ( p && p->lpRemoteName )
	{
		return FSString( Utf16ToUnicode( p->lpRemoteName ).data() );
	}

	return FSString( "Network" );
}

FSWin32Net::~FSWin32Net()
{
}

#else

#include <pwd.h>
#include <grp.h>

#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>

// for statfs()
#ifdef __linux__
#  include <sys/statfs.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
#  include <sys/param.h>
#  include <sys/mount.h>
#endif

#ifdef __linux__
#  define OPENFLAG_LARGEFILE (O_LARGEFILE)
#else
#  define OPENFLAG_LARGEFILE (0)
#endif

unsigned FSSys::Flags() { return HAVE_READ | HAVE_WRITE | HAVE_SYMLINK | HAVE_SEEK; }
bool  FSSys::IsEEXIST( int err ) { return err == EEXIST; }
bool  FSSys::IsENOENT( int err ) { return err == ENOENT; }
bool  FSSys::IsEXDEV( int err ) { return err == EXDEV; }

FSString FSSys::StrError( int err )
{
	sys_char_t buf[1024] = "";
	FSString ret( sys_charset_id, ( char* )sys_error_str( err, buf, sizeof( buf ) ) );
	return ret;
}

bool FSSys::Equal( FS* fs )
{
	if ( !fs || fs->Type() != FS::SYSTEM ) { return false; }

	return true;
}


int FSSys::OpenRead  ( FSPath& path, int flags, int* err, FSCInfo* info )
{
	int n =  open( ( char* ) path.GetString( sys_charset_id, '/' ),
	               O_RDONLY | OPENFLAG_LARGEFILE, 0 );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	return n;
}


int FSSys::OpenCreate   ( FSPath& path, bool overwrite, int mode, int flags,   int* err, FSCInfo* info )
{
	int n =  open( ( char* ) path.GetString( sys_charset_id, '/' ),
	               O_CREAT | O_WRONLY | O_TRUNC | OPENFLAG_LARGEFILE | ( overwrite ? 0 : O_EXCL ) , mode );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	return n;
}

int FSSys::Close( int fd, int* err, FSCInfo* info )
{
	if ( close( fd ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}

int FSSys::Read( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	int n = read( fd, buf, size );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	return n;
}

int FSSys::Seek( int fd, SEEK_FILE_MODE mode, seek_t pos, seek_t* pRet,  int* err, FSCInfo* info )
{

	seek_t n =  lseek( fd, pos, mode );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	if ( pRet ) { *pRet = n; }

	return 0;
}


int FSSys::Write( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	int n = write( fd, buf, size );

	if ( n < 0 ) { SetError( err, errno ); return -1; }

	return n;
}

int FSSys::Rename ( FSPath&  oldpath, FSPath& newpath, int* err,  FSCInfo* info )
{
	if ( rename( ( char* ) oldpath.GetString( sys_charset_id, '/' ), ( char* ) newpath.GetString( sys_charset_id, '/' ) ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}

int FSSys::MkDir( FSPath& path, int mode, int* err,  FSCInfo* info )
{
	if ( mkdir( ( char* ) path.GetString( sys_charset_id, '/' ), mode ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;

}

int FSSys::Delete( FSPath& path, int* err, FSCInfo* info )
{
	if ( unlink( ( char* ) path.GetString( sys_charset_id, '/' ) ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}

int FSSys::RmDir( FSPath& path, int* err, FSCInfo* info )
{
	if ( rmdir( ( char* ) path.GetString( sys_charset_id, '/' ) ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}


int FSSys::SetFileTime  ( FSPath& path, FSTime cTime, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )
{
	struct timeval tv[2];
	tv[0].tv_sec  = aTime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec  = mTime;
	tv[1].tv_usec = 0;

	if ( utimes( ( char* ) path.GetString( sys_charset_id, '/' ), tv ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;

}

int FSSys::ReadDir( FSList* list, FSPath& _path, int* err, FSCInfo* info )
{
	list->Clear();

	FSPath path( _path );

	DIR* d = opendir( ( char* )path.GetString( sys_charset_id ) );

	if ( !d )
	{
		SetError( err, errno );
		return -1;
	}

	try
	{
		struct dirent ent, *pEnt;

		int n = path.Count();

		while ( true )
		{
			if ( info && info->Stopped() )
			{
				closedir( d );
				return -2;
			}

			if ( readdir_r( d, &ent, &pEnt ) )
			{
				SetError( err, errno );
				closedir( d );
				return -1;
			}

			if ( !pEnt ) { break; }

			//skip . and ..
			if ( ent.d_name[0] == '.' && ( !ent.d_name[1] || ( ent.d_name[1] == '.' && !ent.d_name[2] ) ) )
			{
				continue;
			}

			clPtr<FSNode> pNode = new FSNode();
			path.SetItem( n, sys_charset_id, ent.d_name );
			Stat( path, &pNode->st, 0, info );
#if defined(__APPLE__)
			if ( sys_charset_id == CS_UTF8 )
			{
				std::string normname = normalize_utf8_NFC( ent.d_name );
				pNode->name.Set( sys_charset_id, normname.data() );
			}
			else
			{
				pNode->name.Set( sys_charset_id, ent.d_name );
			}
#else
			pNode->name.Set( sys_charset_id, ent.d_name );
#endif
			list->Append( pNode );
		};

		closedir( d );

		return 0;
	}
	catch ( ... )
	{
		closedir( d );
		throw;
	}
}

int64_t FSSys::GetFileSystemFreeSpace( FSPath& path, int* err )
{
#if defined( __linux__ ) && !defined( __APPLE__ )
	struct statfs64 s;

	if ( statfs64( path.GetUtf8(), &s ) == -1 )
	{
		SetError( err, errno );
		return -1;
	}

#else
	// FreeBSD and probably other systems have 64 bit support in regular statfs
	struct statfs s;

	if ( statfs( path.GetUtf8(), &s ) == -1 )
	{
		SetError( err, errno );
		return -1;
	}

#endif

	return ( int64_t )( s.f_bfree ) * ( int64_t )( s.f_bsize );
}

int FSSys::Stat( FSPath& path, FSStat* fsStat, int* err, FSCInfo* info )
{
	fsStat->link.Clear();

#ifdef S_IFLNK
	struct stat st_link;

	if ( lstat( ( char* )path.GetString( sys_charset_id ), &st_link ) )
	{
		SetError( err, errno );
		return -1;
	};

	if ( ( st_link.st_mode & S_IFMT ) == S_IFLNK )
	{
		char buf[1024];
		ssize_t ret = readlink( ( char* )path.GetString( sys_charset_id ), buf, sizeof( buf ) );

		if ( ret >= sizeof( buf ) ) { ret = sizeof( buf ) - 1; }

		if ( ret >= 0 ) { buf[ret] = 0; }
		else { buf[0] = 0; }

		if ( ret >= 0 ) { fsStat->link.Set( sys_charset_id, buf ); }
	}
	else
	{
		fsStat->mode = st_link.st_mode;
		fsStat->size   = st_link.st_size;
		fsStat->m_CreationTime = st_link.st_ctime;
		fsStat->m_LastAccessTime = st_link.st_atime;
		fsStat->m_LastWriteTime = st_link.st_mtime;
		fsStat->m_ChangeTime = st_link.st_mtime;
		fsStat->gid = st_link.st_gid;
		fsStat->uid = st_link.st_uid;

		fsStat->dev = st_link.st_dev;
		fsStat->ino = st_link.st_ino;

		return 0;
	}
#endif

	struct stat st;

	if ( stat( ( char* )path.GetString( sys_charset_id ), &st ) )
	{
		SetError( err, errno );
		return -1;
	}

	fsStat->mode = st.st_mode;
	fsStat->size   = st.st_size;
	fsStat->m_CreationTime = st.st_ctime;
	fsStat->m_LastAccessTime = st.st_atime;
	fsStat->m_LastWriteTime = st.st_mtime;
	fsStat->m_ChangeTime = st.st_mtime;
	fsStat->gid = st.st_gid;
	fsStat->uid = st.st_uid;

	fsStat->dev = st.st_dev;
	fsStat->ino = st.st_ino;

	return 0;
}

int FSSys::StatSetAttr( FSPath& path, const FSStat* st, int* err, FSCInfo* info )
{
	// TODO: Complete - at least, add a mc-style chown.
	int status = chmod( path.GetUtf8(), (mode_t) st->mode );
	if ( status != 0)
	{
		SetError( err, errno );
	}
	return status;
}

int FSSys::FStat( int fd, FSStat* fsStat, int* err, FSCInfo* info )
{
	fsStat->link.Clear();

	struct stat st;

	if ( fstat( fd, &st ) )
	{
		SetError( err, errno );
		return -1;
	}

	fsStat->mode = st.st_mode;
	fsStat->size   = st.st_size;
	fsStat->m_CreationTime = st.st_ctime;
	fsStat->m_LastAccessTime = st.st_atime;
	fsStat->m_LastWriteTime = st.st_mtime;
	fsStat->m_ChangeTime = st.st_mtime;
	fsStat->gid = st.st_gid;
	fsStat->uid = st.st_uid;

	fsStat->dev = st.st_dev;
	fsStat->ino = st.st_ino;

	return 0;
}


int FSSys::Symlink( FSPath& path, FSString& str, int* err, FSCInfo* info )
{
	if ( symlink( ( char* )str.Get( sys_charset_id ), ( char* )path.GetString( sys_charset_id ) ) )
	{
		SetError( err, errno );
		return -1;
	}

	return 0;
}

static std::unordered_map<int, std::vector<unicode_t> > userList;
static std::unordered_map<int, std::vector<unicode_t> > groupList;

#include <sys/statvfs.h>

int FSSys::StatVfs( FSPath& path, FSStatVfs* vst, int* err, FSCInfo* info )
{
	struct statvfs st;

	if ( statvfs( ( char* )path.GetString( sys_charset_id ), &st ) )
	{
		SetError( err, errno );
		return -1;
	}

	vst->size = int64_t( st.f_blocks ) * st.f_frsize;
	vst->avail = int64_t( st.f_bavail ) * st.f_frsize;

	return 0;
}

FSString FSSys::Uri( FSPath& path )
{
	return FSString( path.GetUnicode() );
}

static std::vector<unicode_t> GetOSUserName( int id )
{
	auto i = userList.find( id );

	if ( i != userList.end() ) { return i->second; }

	setpwent();
	struct passwd* p = getpwuid( id );
	char buf[64];
	char* nameStr = p ? p->pw_name : buf;

	if ( !p ) { sprintf( buf, "%i", id ); }

	std::vector<unicode_t> str = sys_to_unicode_array( nameStr );
	userList[id] = str;
	endpwent();
	return str;
}

static std::vector<unicode_t> GetOSGroupName( int id )
{
	auto i = groupList.find( id );

	if ( i != groupList.end() ) { return i->second; }

	setgrent();
	struct group* p = getgrgid( id );
	char buf[64];
	char* nameStr = p ? p->gr_name : buf;

	if ( !p ) { sprintf( buf, "%i", id ); }

	std::vector<unicode_t> str = sys_to_unicode_array( nameStr );
	groupList[id] = str;
	endgrent();
	return str;
}

unicode_t* FSSys::GetUserName( int user, unicode_t buf[64] )
{
	std::vector<unicode_t> Name = GetOSUserName( user );
	unicode_t* u = Name.data();
	unicode_t* s = buf;

	for ( int n = 63; n > 0 && *u; n--, u++, s++ )
	{
		*s = *u;
	}

	*s = 0;
	return buf;
};


unicode_t* FSSys::GetGroupName( int group, unicode_t buf[64] )
{
	std::vector<unicode_t> Name = GetOSGroupName( group );

	unicode_t* g = Name.data();
	unicode_t* s = buf;

	for ( int n = 63; n > 0 && *g; n--, g++, s++ )
	{
		*s = *g;
	}

	*s = 0;

	return buf;
};

#endif

FSSys::~FSSys() {}


//////////////////////////////////////// FSList /////////////////////////////////////

void FSList::Append( clPtr<FSNode> p )
{
	p->next = 0;

	if ( last )
	{
		last->next = p.ptr();
	}
	else
	{
		first = p.ptr();
	}

	last = p.ptr();
	p.drop();
	count++;
}

void FSList::Clear()
{
	for ( FSNode* p = first; p; )
	{
		FSNode* t = p;
		p = p->next;
		delete t;
	}

	first = last = 0;
	count = 0;
}

void FSList::CopyFrom( const FSList& a, bool onlySelected )
{
	Clear();

	for ( FSNode* p = a.first; p; p = p->next )
	{
		if ( onlySelected && !p->isSelected ) { continue; }

		FSNode* node = new FSNode( *p );

		if ( last )
		{
			last->next = node;
		}
		else
		{
			first = node;
		}

		last = node;
		count++;
	}
}

void FSList::CopyOne( FSNode* node )
{
	Clear();
	FSNode* p = new FSNode( *node );
	first = last = p;
	count = 1;
}

std::vector<FSNode*> FSList::GetArray()
{
	std::vector<FSNode*> p( Count() );
	FSNode* pNode = first;
	int n = Count();

	for ( int i = 0 ; i < n && pNode; i++, pNode = pNode->next )
	{
		p[i] = pNode;
	}

	return p;
}

std::vector<FSNode*> FSList::GetFilteredArray( bool showHidden, int* pCount )
{
	if ( pCount ) { *pCount = 0; }

	std::vector<FSNode*> p( Count() );
	FSNode* pNode = first;
	int n = Count();
	int i;

	for ( i = 0 ; i < n && pNode; pNode = pNode->next )
		if ( showHidden || !pNode->IsHidden() )
		{
			p[i++] = pNode;
		}

	if ( pCount ) { *pCount = i; }

	return p;
}


// binary search. 
// Returns index of the found node
// in EXACT_MATCH_ONLY mode returns -1 if node not found
// in EXACT_OR_CLOSEST_PRECEDING_NODE modes returns -1 if n is less than the 1st nodeVector element 
// in EXACT_OR_CLOSEST_SUCCEEDING_NODE modes returns vsize if n is greater than the last nodeVector element
static int _BSearch(FSNode& n, const std::vector<FSNode*>& nodeVector, int(*CmpFunc)(FSNode* n1, FSNode* n2), BSearchMode bSearchMode)
{
	int vsize = nodeVector.size();
	if (vsize == 0)
		return -1;

	int iLeft = 0;
	int iRight = vsize-1;
	//int cmp = CmpFunc(&n, nodeVector[iLeft]);

	for (int i = iRight / 2; iLeft <= iRight ; i = (iLeft + iRight) / 2)
	{
		int cmp = CmpFunc(&n, nodeVector[i]);
		if (cmp == 0) // found exact match
			return i;
		if (cmp > 0)
			iLeft = i + 1;
		else
			iRight = i - 1;
	}

	switch (bSearchMode)
	{
	case EXACT_OR_CLOSEST_PRECEDING_NODE:
		return iRight;
	case EXACT_OR_CLOSEST_SUCCEEDING_NODE:
		return iLeft;
	default:
	case EXACT_MATCH_ONLY:
		return -1;
	}
}


// standard: returns n1 - n2, for bsearch
template <bool isAscending, bool isCaseSensitive, SORT_MODE mode>
int CmpFunc(FSNode* a, FSNode* b)
{
	// directories always go first
	int dirCmp = a->IsDir() - b->IsDir();
	if (dirCmp)
		return -dirCmp;
	switch (mode)
	{
	case SORT_EXT:
	{
								int cmpExt = a->CmpByExt(*b, isCaseSensitive);
								if (cmpExt)
									return isAscending ? cmpExt : -cmpExt;
	} // if extensions match, do name comparison using current ascending mode
	  // i.e. fall into the next case
	case SORT_NAME:
	{
								int cmpName = isCaseSensitive ? a->name.Cmp(b->name) : a->name.CmpNoCase(b->name);
								return isAscending ? cmpName : -cmpName;
	}
	case SORT_SIZE:
	{
								 int64_t cmpSize = a->st.size - b->st.size;
								 if (cmpSize)
									 return isAscending ? (cmpSize > 0 ? 1 : -1) : (cmpSize > 0 ? -1 : 1);
								 break;
	}
	case SORT_MTIME:
	{
								  time_t cmpTime = a->st.m_LastWriteTime - b->st.m_LastWriteTime;
								  if (cmpTime)
									  return isAscending ? (cmpTime > 0 ? 1 : -1) : (cmpTime > 0 ? -1 : 1);
								  break;
	}
	default:
		return 0;
	}
	// if size|mtime match, return name comparison in ascending=true mode
	return  isCaseSensitive ? a->name.Cmp(b->name) : a->name.CmpNoCase(b->name);
}

FSNodeCmpFunc* FSNodeVectorSorter::getCmpFunc(bool isAscending, bool isCaseSensitive, SORT_MODE sortMode)
{
	switch (sortMode)
	{
	case SORT_NAME:
		return isAscending ?
			(isCaseSensitive ? CmpFunc<true, true, SORT_NAME> : CmpFunc<true, false, SORT_NAME>) :
			(isCaseSensitive ? CmpFunc<false, true, SORT_NAME> : CmpFunc<false, false, SORT_NAME>);
	case SORT_EXT:
		return isAscending ?
			(isCaseSensitive ? CmpFunc<true, true, SORT_EXT> : CmpFunc<true, false, SORT_EXT>) :
			(isCaseSensitive ? CmpFunc<false, true, SORT_EXT> : CmpFunc<false, false, SORT_EXT>);
		break;
	case SORT_SIZE:
		return isAscending ? CmpFunc<true, true, SORT_SIZE> : CmpFunc<false, true, SORT_SIZE>;
		break;
	case SORT_MTIME:
		return isAscending ? CmpFunc<true, true, SORT_MTIME> : CmpFunc<false, true, SORT_MTIME>;
	default: // SORT_NONE
		break;
	}
	return nullptr;
}

void FSNodeVectorSorter::Sort(std::vector<FSNode*>& nodeVector, 
	bool isAscending, bool isCaseSensitive, SORT_MODE sortMode)
{
	struct GreaterCmp
	{
		FSNodeCmpFunc* pCmpFunc;
		GreaterCmp(FSNodeCmpFunc* _pCmpFunc) : pCmpFunc(_pCmpFunc){}
		bool operator() (FSNode* n1, FSNode* n2)
		{
			return pCmpFunc(n1, n2) < 0;
		}
	};

	FSNodeCmpFunc* pFunc = getCmpFunc(isAscending, isCaseSensitive, sortMode);
	if (pFunc)
	{
		std::sort(nodeVector.begin(), nodeVector.end(), GreaterCmp(pFunc));
	}
}

int FSNodeVectorSorter::BSearch(FSNode& n, const std::vector<FSNode*>& nodeVector, 
	BSearchMode searchMode, bool isAscending, bool isCaseSensitive, SORT_MODE sortMode)
{
	FSNodeCmpFunc *pFunc = getCmpFunc(isAscending, isCaseSensitive, sortMode);
	if (pFunc)
		return _BSearch(n, nodeVector, pFunc, searchMode);
	else
		return -1;
}

/////////////////////////////////////  FSStat ////////////////////////////////////////


unicode_t* FSStat::GetModeStr( unicode_t buf[64] )
{
	unicode_t* p = buf;
	/* print type */

	if ( IsLnk() ) { *p++ = '>'; }

	switch ( mode & S_IFMT )
	{
		case S_IFDIR:        /* directory */
			*p++ = 'd';
			break;

		case S_IFCHR:        /* character special */
			*p++ = 'c';
			break;

		case S_IFBLK:        /* block special */
			*p++ = 'b';
			break;

		case S_IFREG:        /* regular */
			*p++ = '-';
			break;
#ifdef S_IFLNK

		case S_IFLNK:        /* symbolic link */
			*p++ = 'l';
			break;
#endif
#ifdef S_IFSOCK

		case S_IFSOCK:       /* socket */
			*p++ = 's';
			break;
#endif
#ifdef S_IFIFO

		case S_IFIFO:        /* fifo */
			*p++ = 'p';
			break;
#endif

		default:       /* unknown */
			*p++ = '?';
			break;
	}

	/* usr */
	if ( mode & S_IRUSR )
	{
		*p++ = 'r';
	}
	else
	{
		*p++ = '-';
	}

	if ( mode & S_IWUSR )
	{
		*p++ = 'w';
	}
	else
	{
		*p++ = '-';
	}

	switch ( mode & ( S_IXUSR | S_ISUID ) )
	{
		case 0:
			*p++ = '-';
			break;

		case S_IXUSR:
			*p++ = 'x';
			break;

		case S_ISUID:
			*p++ = 'S';
			break;

		case S_IXUSR | S_ISUID:
			*p++ = 's';
			break;
	}

	/* group */
	if ( mode & S_IRGRP )
	{
		*p++ = 'r';
	}
	else
	{
		*p++ = '-';
	}

	if ( mode & S_IWGRP )
	{
		*p++ = 'w';
	}
	else
	{
		*p++ = '-';
	}

	switch ( mode & ( S_IXGRP | S_ISGID ) )
	{
		case 0:
			*p++ = '-';
			break;

		case S_IXGRP:
			*p++ = 'x';
			break;

		case S_ISGID:
			*p++ = 'S';
			break;

		case S_IXGRP | S_ISGID:
			*p++ = 's';
			break;
	}

	/* other */
	if ( mode & S_IROTH )
	{
		*p++ = 'r';
	}
	else
	{
		*p++ = '-';
	}

	if ( mode & S_IWOTH )
	{
		*p++ = 'w';
	}
	else
	{
		*p++ = '-';
	}

	switch ( mode & ( S_IXOTH | S_ISVTX ) )
	{
		case 0:
			*p++ = '-';
			break;

		case S_IXOTH:
			*p++ = 'x';
			break;

		case S_ISVTX:
			*p++ = 'T';
			break;

		case S_IXOTH | S_ISVTX:
			*p++ = 't';
			break;
	}

	*p++ = ' ';    /* will be a '+' if ACL's implemented */
	*p = '\0';
	return buf;
}

std::string GetFSTimeStrTime( FSTime TimeValue )
{
	char str[64];
#ifdef _WIN32
	FILETIME mt = TimeValue;
	FILETIME lt;
	SYSTEMTIME st;

	if (!FileTimeToLocalFileTime(&mt, &lt) || !FileTimeToSystemTime(&lt, &st))
	{
		return std::string("?");
	}

	Lsnprintf( str, sizeof(str), "%02i:%02i:%02i,%03i", int(st.wHour), int(st.wMinute), int(st.wSecond), int(st.wMilliseconds) );
#else
	time_t mt = TimeValue;
	struct tm* p = localtime(&mt);

	if (p)
	{
		sprintf( str, "%02i:%02i:%02i,%03i", p->tm_hour, p->tm_min, p->tm_sec, int(0) );
	}
	else
	{
		sprintf( str, "%02i:%02i:%02i,%03i", int(0), int(0), int(0), int(0) );
	}
#endif
	return std::string( str );
}

std::string GetFSTimeStrDate( FSTime TimeValue )
{
	char str[64];
#ifdef _WIN32
	FILETIME mt = TimeValue;
	FILETIME lt;
	SYSTEMTIME st;

	if ( !FileTimeToLocalFileTime(&mt, &lt) || !FileTimeToSystemTime(&lt, &st) )
	{
		return std::string("?");
	}

	Lsnprintf(str, sizeof(str), "%02i.%02i.%04i", int( st.wDay ), int( st.wMonth ), int( st.wYear ) );

#else
	time_t mt = TimeValue;
	struct tm* p = localtime(&mt);

	if ( p )
	{
		sprintf( str, "%02i.%02i.%04i", p->tm_mday, p->tm_mon + 1, p->tm_year + 1900 );
	}
	else
	{
		sprintf(str, "%02i.%02i.%04i", int(0), int(0), int(0) + 1900 );

	}
#endif
	return std::string(str);
}

FSTime GetFSTimeFromStr( const std::string& Date, const std::string& Time )
{
	if ( Date.empty() || Time.empty() ) return FSTime();

	int Day = 0;
	int Month = 0;
	int Year = 0;
	int Hour = 0;
	int Minute = 0;
	int Second = 0;
	int Milliseconds = 0;

	if ( Lsscanf( Date.c_str(), "%i.%i.%i", &Day, &Month, &Year ) != 3 ) return FSTime();
	int NumValues = Lsscanf( Time.c_str(), "%i:%i:%i,%i", &Hour, &Minute, &Second, &Milliseconds );
	if ( NumValues != 3 && NumValues != 4 ) return FSTime();

	FSTime Result;

#if defined(_WIN32)
	SYSTEMTIME SystemTime;
	memset( &SystemTime, 0, sizeof(SystemTime) );
	SystemTime.wDay = Day;
	SystemTime.wMonth = Month;
	SystemTime.wYear = Year;
	SystemTime.wHour = Hour;
	SystemTime.wMinute = Minute;
	SystemTime.wSecond = Second;
	SystemTime.wMilliseconds = Milliseconds;

	FILETIME LocalTime;
	FILETIME FileTime;

	if ( !SystemTimeToFileTime( &SystemTime, &LocalTime ) ) return FSTime();
	if ( !LocalFileTimeToFileTime( &LocalTime, &FileTime ) ) return FSTime();

	Result = FileTime;
#else
	tm p;
	p.tm_mday = Day;
	p->tm_mon = Month - 1;
	p->tm_year = Year - 1900;
	p->tm_hour = Hour;
	p->tm_min = Minute;
	p->tm_sec = Second;

	time_t mt = mktime( &p );

	Result = mt;
#endif

	return Result;
}

unicode_t* FSTimeToStr( unicode_t ret[64], FSTime TimeValue )
{
	char str[64];
	unicode_t* t = ret;
#ifdef _WIN32
	FILETIME mt = TimeValue;
	FILETIME lt;
	SYSTEMTIME st;

	if ( !FileTimeToLocalFileTime( &mt, &lt ) || !FileTimeToSystemTime( &lt, &st ) ) { ret[0] = '?'; ret[1] = 0; return ret; }

	Lsnprintf( str, sizeof( str ), "%02i.%02i.%04i  %02i:%02i:%02i",
	           int( st.wDay ), int( st.wMonth ), int( st.wYear ),
	           int( st.wHour ), int( st.wMinute ), int( st.wSecond ) );
#else
	time_t mt = TimeValue;
	struct tm* p = localtime( &mt );

	if ( p )
	{
		sprintf( str, "%02i.%02i.%04i  %02i:%02i:%02i",
		         p->tm_mday, p->tm_mon + 1, p->tm_year + 1900, // % 100,
		         p->tm_hour, p->tm_min, p->tm_sec );
	}
	else
	{
		sprintf( str, "%02i.%02i.%04i  %02i:%02i:%02i",
		         int( 0 ), int( 0 ), int( 0 ) + 1900, // % 100,
		         int( 0 ), int( 0 ), int( 0 ) );

	}

#endif

	for ( char* s = str; *s; s++, t++ )
	{
		*t = *s;
	}

	*t = 0;
	return ret;
}

unicode_t* FSStat::GetMTimeStr( unicode_t ret[64] )
{
	return FSTimeToStr( ret, m_LastWriteTime );
}

unicode_t* FSStat::GetPrintableSizeStr( unicode_t buf[64] )
{
	unicode_t str[10];
	str[0] = 0;

	seek_t num = size;

	if ( num >= seek_t( 10l ) * 1024 * 1024 * 1024 )
	{
		num /= seek_t( 1024l ) * 1024 * 1024;
		str[0] = ' ';
		str[1] = 'G';
		str[2] = 0;
	}
	else if ( num >= 10l * 1024 * 1024 )
	{
		num /= 1024 * 1024;
		str[0] = ' ';
		str[1] = 'M';
		str[2] = 0;
	}
	else if ( num >= 1024 * 1024 )
	{
		num /= 1024;
		str[0] = ' ';
		str[1] = 'K';
		str[2] = 0;
	}

	
	std::string Str = ToString( num );

	unicode_t* us = buf;

	for ( const char* s = Str.c_str(); *s; s++ )
	{
		*( us++ ) = *s;
	}

	for ( unicode_t* t = str; *t; t++ )
	{
		*( us++ ) = *t;
	}

	*us = 0;

	return buf;
}


/////////////////////////////////////  FSNode ////////////////////////////////////////

inline const unicode_t* unicode_rchr( const unicode_t* s, int c )
{
	const unicode_t* p = 0;

	if ( s )
		for ( ; *s; s++ ) if ( *s == c ) { p = s; }

	return p;
}

inline int CmpNoCase( const unicode_t* a, const unicode_t* b )
{
	unicode_t au = 0;
	unicode_t bu = 0;

	for ( ; *a; a++, b++ )
	{
		au = UnicodeLC( *a );
		bu = UnicodeLC( *b );

		if ( au != bu ) { break; }
	};

	return ( *a ? ( *b ? ( au < bu ? -1 : ( au == bu ? 0 : 1 ) ) : 1 ) : ( *b ? -1 : 0 ) );
}


#ifdef _WIN32

bool FSNode::IsExe()
{
	const unicode_t* s = unicode_rchr( GetUnicodeName(), '.' );

	if ( !s || !*s ) { return false; }

	s++;

	if ( UnicodeLC( s[0] ) == 'e' && UnicodeLC( s[1] ) == 'x' && UnicodeLC( s[2] ) == 'e' && !s[3] ) { return true; }

	if ( UnicodeLC( s[0] ) == 'b' && UnicodeLC( s[1] ) == 'a' && UnicodeLC( s[2] ) == 't' && !s[3] ) { return true; }

	if ( UnicodeLC( s[0] ) == 'c' && UnicodeLC( s[1] ) == 'm' && UnicodeLC( s[2] ) == 'd' && !s[3] ) { return true; }

	return false;
}

#endif



int FSNode::CmpByExt( FSNode& a, bool case_sensitive )
{
	const unicode_t* s1 = unicode_rchr( GetUnicodeName(), '.' );
	const unicode_t* s2 = unicode_rchr( a.GetUnicodeName(), '.' );

	if ( s1 )
		return ( s2 ) ?
		       ( case_sensitive ? CmpStr<const unicode_t>( s1, s2 ) : CmpNoCase( s1, s2 ) )
		       : 1;
	else
	{
		return s2 ? -1 : 0;
	}
}



FSNode::~FSNode() {}

////////////////////////////////////  Utils ///////////////////////////////////////////

bool ParzeLink( FSPath& path, FSString& link )
{
	FSPath t( link );

	if ( !path.IsAbsolute() && !t.IsAbsolute() ) { return false; } //not absolute path

	int first = 0;

	if ( t.IsAbsolute() )
	{
		path.Clear();
		path.PushStr( FSString( "" ) );
		first = 1;
	}

	for ( int i = first; i < t.Count(); i++ )
	{
		FSString p = *( t.GetItem( i ) );

		if ( p.IsDot() ) { continue; }

		if ( p.Is2Dot() )
		{
			if ( path.Count() > 1 ) { path.Pop(); }
		}
		else
		{
			path.PushStr( p );
		}
	}

	return true;
}
