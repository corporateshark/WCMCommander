/*
 * Part of WCM Commander
 * https://github.com/corporateshark/WCMCommander
 * walcommander@linderdaum.com
 */

#include <algorithm>

#include "vfs-ftp.h"

#include "swl.h"
#include "string-util.h"

#ifdef _WIN32
#include <time.h>
#endif

unicode_t FtpIDCollection::u0 = 0;


static bool TcpStopChecker( void* p )
{
	if ( !p ) { return false; }

	if ( ( ( FSCInfo* )p )->Stopped() )
	{
//printf("\nTCP stopped\n");
		return true;
	}

	return false;
}

class CStopSetter
{
	FTPNode* node;
public:
	CStopSetter( FTPNode* n, FSCInfo* info ): node( n ) { if ( node ) { node->SetStopFunc( TcpStopChecker, info ); } }
	~CStopSetter() { if ( node ) { node->ClearStopFunc(); } }
};


static int GETINT( const char* s, const char** pLast )
{
	if ( !s ) { return -1; }

	int n = 0;

	for ( ; *s >= '0' && *s <= '9'; s++ )
	{
		n = n * 10 + ( *s - '0' );
	}

	if ( pLast ) { *pLast = s; }

	return n;
}

static int SKIPINT( const char*& s )
{
	if ( !s ) { return -1; }

	int n = 0;

	for ( ; *s >= '0' && *s <= '9'; s++ )
	{
		n = n * 10 + ( *s - '0' );
	}

	return n;
}


inline void CheckFtpRet( int ret )
{
	if ( ret < 100 ) { throw int ( EFTP_BADPROTO ); }

	if ( ret > 299 ) { throw int ( -ret ); }
}


int FTPNode::ReadCode()
{
	const char* last = nullptr;
	char buf[0x100];
	int r = GETINT( ctrl.ReadLine( buf, sizeof( buf ) ), &last );

	if ( *last != '-' ) { return r; }

	if ( r < 100 ) { return r; }

	for ( int i = 0; i < 1000; i++ ) // >1000 - bad return :)
	{
		int r2 = GETINT( ctrl.ReadLine( buf, sizeof( buf ) ), &last );

		if ( r2 == r && *last != '-' ) { return r; }
	};

	return 1; //bad proto

	/*
	   while (true)
	   {
	      const char *last;
	      char buf[0x100];
	      int r = GETINT(ctrl.ReadLine(buf, sizeof(buf)), &last);
	      if (*last != '-') return r;
	   }
	//*/
};

void FTPNode::Passive( unsigned& passiveIp, int& passivePort )
{
	ctrl.WriteStr( "PASV\r\n" );
	char buf[0x100];
	ctrl.ReadLine( buf, sizeof( buf ) );
	const char* s = buf;
	int rc = SKIPINT( s );
	CheckFtpRet( rc );
	s = strchr( s, '(' );

	if ( !s ) { throw int ( EFTP_BADPROTO ); }

	s++;
	int ip1 = SKIPINT( s );

	if ( *s != ',' ) { throw int ( EFTP_BADPROTO ); }

	s++;
	int ip2 = SKIPINT( s );

	if ( *s != ',' ) { throw int ( EFTP_BADPROTO ); }

	s++;
	int ip3 = SKIPINT( s );

	if ( *s != ',' ) { throw int ( EFTP_BADPROTO ); }

	s++;
	int ip4 = SKIPINT( s );

	if ( *s != ',' ) { throw int ( EFTP_BADPROTO ); }

	s++;
	int port1 = SKIPINT( s );

	if ( *s != ',' ) { throw int ( EFTP_BADPROTO ); }

	s++;
	int port2 = SKIPINT( s );

	if ( *s != ')' ) { throw int ( EFTP_BADPROTO ); }

	s++;
	passiveIp = ( unsigned( ip1 & 0xFF ) << 24 ) | ( unsigned( ip2 & 0xFF ) << 16 ) | ( unsigned( ip3 & 0xFF ) << 8 ) | ( unsigned( ip4 & 0xFF ) );
	passivePort = ( unsigned( port1 & 0xFF ) << 8 ) | ( unsigned( port2 & 0xFF ) );
}

void FTPNode::Port( unsigned ip, int port )
{
	char buf[0x100];
	Lsnprintf( buf, sizeof( buf ), "PORT %i,%i,%i,%i,%i,%i\r\n",
	           ( ip >> 24 ) & 0xFF, ( ip >> 16 ) & 0xFF, ( ip >> 8 ) & 0xFF, ip & 0xFF,
	           ( port >> 8 ) & 0xFF, port & 0xFF );

	ctrl.WriteStr( buf );
	CheckFtpRet( ReadCode() );
}

void FTPNode::OpenData( const char* cmd )
{
	data.Close();
	unsigned ip;
	int port;
	TCPSock s;

	if ( _passive )
	{
		Passive( ip, port );
	}
	else
	{
		s.Create();
		s.SetNonBlock();
		s.Bind();
		Sin ctrlSin, sSin;
		ctrl.GetSockName( ctrlSin );
		s.GetSockName( sSin );
		Port( ctrlSin.Ip(), sSin.Port() );
	};

	ctrl.WriteStr( cmd );

	ctrl.Flush();

	if ( _passive )
	{
		data.Connect( ip, port );
	}
	else
	{
		s.Listen();

		if ( !s.SelectRead( TIMEOUT_ACCEPT ) ) { throw int( -3 ); } //!!! every seconds loop is needed

		Sin sin;
		s.Accept( data.Sock(), sin );
	}

	CheckFtpRet( ReadCode() );
}


FTPNode::FTPNode(): _passive( false ) {}

void FTPNode::Connect( unsigned ip, int port, const char* user, const char* password, bool passive )
{
	ctrl.Close( true );
	data.Close( true );
	_passive = passive;
	ctrl.Connect( ip, port );
	int rc = ReadCode();
	CheckFtpRet( rc );
	ctrl.WriteStr( "USER ", user, "\r\n" );
	rc = ReadCode();

	if ( rc == 331 )
	{
		ctrl.WriteStr( "PASS ", password, "\r\n" );
		rc = ReadCode();
	}

	CheckFtpRet( rc );
	ctrl.WriteStr( "TYPE I\r\n" );
	CheckFtpRet( ReadCode() );
}

void FTPNode::Noop()
{
	ctrl.WriteStr( "NOOP\r\n" );
	CheckFtpRet( ReadCode() );
}

void FTPNode::Ls( ccollect<std::string >& list )
{
	OpenData( "LIST\r\n" );
	list.clear();
	char buf[4096];

	while ( data.ReadLine( buf, sizeof( buf ) ) )
	{
		list.append( std::string( buf ) );
	}

	data.Close();
	CheckFtpRet( ReadCode() ); //250
}

void FTPNode::Cwd( const char* path )
{
	ctrl.WriteStr( "CWD ", path, "\r\n" );
	CheckFtpRet( ReadCode() );
}

void FTPNode::OpenRead( const char* fileName )
{
	char buf[4096];
	Lsnprintf( buf, sizeof( buf ) - 1, "RETR %s\r\n", fileName );
	OpenData( buf );
}

void FTPNode::OpenWrite( const char* fileName )
{
	char buf[4096];
	Lsnprintf( buf, sizeof( buf ) - 1, "STOR %s\r\n", fileName );
	OpenData( buf );
}

void FTPNode::Delete( const char* fileName )
{
	ctrl.WriteStr( "DELE ", fileName, "\r\n" );
	CheckFtpRet( ReadCode() );
}

void FTPNode::MkDir( const char* fileName )
{
	ctrl.WriteStr( "MKD ", fileName, "\r\n" );
	CheckFtpRet( ReadCode() );
}

void FTPNode::RmDir( const char* fileName )
{
	ctrl.WriteStr( "RMD ", fileName, "\r\n" );
	CheckFtpRet( ReadCode() );
}

void FTPNode::Rename( const char* from, const char* to )
{
	ctrl.WriteStr( "RNFR ", from, "\r\n" );
	int r = ReadCode();

	if ( r != 350 ) { throw int( -r ); }

	ctrl.WriteStr( "RNTO ", to, "\r\n" );
	CheckFtpRet( ReadCode() );
}


void FTPNode::CloseData()
{
	data.Close();
	CheckFtpRet( ReadCode() );
}


FTPNode::~FTPNode() {}



/////////////////////////////////////////////////// FSFtp

int FSFtp::GetFreeNode( int* err, FSCInfo* info )
{
	MutexLock lock( &mutex );

	if ( !_param.isSet && info )
	{
		// unlock the mutex before doing a modal window
		mutex.Unlock();

		bool LogonSuccessful = info->FtpLogon( &_param );

		mutex.Lock();

		if (  LogonSuccessful && _param.isSet )
		{
			MutexLock l1( &infoMutex );
			_infoParam = _param;
		}
		else
		{
			if ( err ) { *err = 0; }

			return -2; //stop
		}
	}

	int n;

	for ( n = 0; n < NODES_COUNT; n++ )
	{
		if ( !nodes[n].busy ) { break; }
	}

	if ( n == NODES_COUNT )
	{
		if ( err ) { *err = EFTP_OUTOFRESOURCE_INTERNAL; }

		return -1;
	}

	/*volatile*/ Node* p = nodes + n;

	p->busy = true;

	lock.Unlock();

	if ( p->pFtpNode.ptr() ) //check
	{
		try
		{
			CStopSetter stopSetter( p->pFtpNode.ptr(), info );

			p->pFtpNode->Noop();
		}
		catch ( int )
		{
			p->pFtpNode = 0;
		}
	}

	if ( !p->pFtpNode.ptr() )
	{
		try
		{
			p->pFtpNode = new FTPNode();

			CStopSetter stopSetter( p->pFtpNode.ptr(), info );

			unsigned ip; // = inet_addr(unicode_to_utf8(_param.server.Data()).ptr());
			int e;

			if ( !GetHostIp( _param.server.c_str(), &ip, &e ) )
			{
				throw int( e );
			}

			FSString User = FSString( _param.user.c_str() );
			FSString Pass = FSString( _param.pass.c_str() );

			const char* UserStr = _param.anonymous ? "anonymous" : (char*)User.Get( _param.charset );
			const char* PassStr = _param.anonymous ? "anonymous@" : (char*)Pass.Get( _param.charset );

			p->pFtpNode->Connect( ntohl( ip ), _param.port, UserStr, PassStr, _param.passive );
		}
		catch ( int e )
		{
			lock.Lock(); //!!!

			p->pFtpNode = 0;
			p->busy = false;

			if ( err ) { *err = e; }

			return ( e == -2 ) ? -2 : -1;
		}
	}

//printf("ftp ft = %i\n", n);
	return n;
}

FSFtp::FSFtp( FSFtpParam* param )
	:  FS( FTP ), statCache( const_cast<int*>( &_param.charset ) )
{
	_param = *param;
	_infoParam = *param;
}

unsigned FSFtp::Flags() { return HAVE_READ; } // | HAVE_WRITE; }


bool FSFtp::IsEEXIST( int err ) { return err == EFTP_EXIST; }
bool FSFtp::IsENOENT( int err ) { return err == EFTP_NOTEXIST; }
bool FSFtp::IsEXDEV( int err ) { return false; }

bool FSFtp::Equal( FS* fs )
{
	if ( !fs || fs->Type() != FS::FTP ) { return false; }

	if ( fs == this ) { return true; }

	FSFtp* f = ( FSFtp* )fs;

	MutexLock l1( &infoMutex );
	MutexLock l2( &( f->infoMutex ) );

	if ( _infoParam.isSet != f->_infoParam.isSet )
	{
		return false;
	}

	return _infoParam.server == f->_infoParam.server &&
	       ( _infoParam.anonymous == f->_infoParam.anonymous && ( _infoParam.anonymous || _infoParam.user == f->_infoParam.user ) ) &&
	       _infoParam.port == f->_infoParam.port &&
	       _infoParam.charset == f->_infoParam.charset;
}

FSString FSFtp::StrError( int err )
{
	if ( err > 0 )
	{
		sys_char_t buf[0x100];
		FSString s;
		s.SetSys( sys_error_str( err, buf, 0x100 ) );
		return s;
	}

	const char* str = "?";
	char buf[0x100];

	switch ( err )
	{
		case EFTP_BADPROTO:
			str = "protocol error";
			break;

		case EFTP_NOTSUPPORTED:
			str = "operation is not supported";
			break;

		case EFTP_OUTOFRESOURCE_INTERNAL:
			str = "out of internal FSFtp resources";
			break;

		case EFTP_INVALID_PARAMETER:
			str = "invelid parameter";
			break;

		case EFTP_NOTEXIST:
			str = "file not exist";
			break;

		case EFTP_EXIST:
			str = "file exist";
			break;


		case -3:
			str = "Operation timed out.";
			break;

		case -331:
			str = "331 User name okay, need password.";
			break;

		case -332:
			str = "332 Need account for login.";
			break;

		case -350:
			str = "350 Requested file action pending further information.";
			break;

		case -421:
			str = "421 Service not available, closing control connection.";
			break;

		case -425:
			str = "425 Can't open data connection.";
			break;

		case -426:
			str = "426 Connection closed; transfer aborted.";
			break;

		case -450:
			str = "450 Requested file action not taken.";
			break;

		case -451:
			str = "451 Requested action aborted. Local error in processing.";
			break;

		case -452:
			str = "452 Requested action not taken.";
			break;

		case -500:
			str = "500 Syntax error, command unrecognized.";
			break;

		case -501:
			str = "501 Syntax error in parameters or arguments.";
			break;

		case -502:
			str = "502 Command not implemented.";
			break;

		case -503:
			str = "503 Bad sequence of commands.";
			break;

		case -504:
			str = "504 Command not implemented for that parameter.";
			break;

		case -530:
			str = "530 Not logged in.";
			break;

		case -532:
			str = "532 Need account for storing files.";
			break;

		case -550:
			str = "550 Requested action not taken.";
			break;

		case -551:
			str = "551 Requested action aborted. Page type unknown.";
			break;

		case -552:
			str = "552 Requested file action aborted.";
			break;

		case -553:
			str = "553 Requested action not taken.";
			break;

		default:
			Lsnprintf( buf, sizeof( buf ), "Unknown error code in FSFtp:%i", err );
			str = buf;
	}

	return FSString( str );
}


int FSFtp::OpenRead  ( FSPath& path, int flags, int* err, FSCInfo* info )
{
	int nodeId = GetFreeNode( err, info );

	if ( nodeId < 0 ) { return nodeId; }

	/*volatile*/ Node* p = nodes + nodeId;

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		p->pFtpNode->OpenRead( ( char* )path.GetString( _param.charset, '/' ) );
	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return nodeId;
}

int FSFtp::OpenCreate   ( FSPath& path, bool overwrite, int mode, int flags, int* err, FSCInfo* info )
{
	if ( !overwrite )
	{
		FSStat st;
		int r = Stat( path, &st, err, info );

		if ( r == -2 ) { return r; }

		if ( !r )
		{
			if ( err ) { *err = EFTP_EXIST; }

			return -1;
		}
	}

	int nodeId = GetFreeNode( err, info );

	if ( nodeId < 0 ) { return nodeId; }

	Node* p = nodes + nodeId;

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		p->pFtpNode->OpenWrite( ( char* )path.GetString( _param.charset, '/' ) );
	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return nodeId;
}

int FSFtp::Close  ( int fd, int* err, FSCInfo* info )
{
	if ( fd < 0 || fd >= NODES_COUNT )
	{
		if ( err ) { *err = EFTP_INVALID_PARAMETER; }

		return -1;
	}

	Node* p = nodes + fd;

	{
		MutexLock lock( &mutex );

		if ( !p->busy )
		{
			if ( err ) { *err = EFTP_INVALID_PARAMETER; }

			return -1;
		}

		lock.Unlock();
	}

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		p->pFtpNode->CloseData();

		{
			MutexLock lock( &mutex );
			p->busy = false;
		}

	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}


int FSFtp::Read   ( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	if ( fd < 0 || fd >= NODES_COUNT )
	{
		if ( err ) { *err = EFTP_INVALID_PARAMETER; }

		return -1;
	}

	Node* p = nodes + fd;

	{
		MutexLock lock( &mutex );

		if ( !p->busy )
		{
			if ( err ) { *err = EFTP_INVALID_PARAMETER; }

			return -1;
		}

		lock.Unlock();
	}

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		int n = p->pFtpNode->ReadData( buf, size );

		{
			// need to add checking if there are operations on file
		}

		return n;

	}
	catch ( int e )
	{
		//busy fd cannot free here
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;
}

int FSFtp::Write  ( int fd, void* buf, int size, int* err, FSCInfo* info )
{
	if ( fd < 0 || fd >= NODES_COUNT )
	{
		if ( err ) { *err = EFTP_INVALID_PARAMETER; }

		return -1;
	}

	Node* p = nodes + fd;

	{
		MutexLock lock( &mutex );

		if ( !p->busy )
		{
			if ( err ) { *err = EFTP_INVALID_PARAMETER; }

			return -1;
		}

		lock.Unlock();
	}

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		p->pFtpNode->WriteData( buf, size );

		{
			// need to add checking if there are operations on file
		}

		return size;

	}
	catch ( int e )
	{
		//busy fd cannot free here
		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	return 0;

}

int FSFtp::Rename ( FSPath&  oldpath, FSPath& newpath, int* err,  FSCInfo* info )
{
	int nodeId = GetFreeNode( err, info );

	if ( nodeId < 0 ) { return nodeId; }

	Node* p = nodes + nodeId;

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		p->pFtpNode->Cwd( "/" );
		p->pFtpNode->Rename( ( char* )oldpath.GetString( _param.charset, '/' ), ( char* )newpath.GetString( _param.charset, '/' ) );

	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	MutexLock lock( &mutex );
	p->busy = false;

	return 0;
}

int FSFtp::MkDir  ( FSPath& path, int mode, int* err,  FSCInfo* info )
{
	{
		//checking for existence
		FSStat st;
		int r = Stat( path, &st, err, info );

		if ( r == -2 ) { return r; }

		if ( !r )
		{
			if ( err ) { *err = EFTP_EXIST; }

			return -1;
		}
	}

	int nodeId = GetFreeNode( err, info );

	if ( nodeId < 0 ) { return nodeId; }

	Node* p = nodes + nodeId;

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		p->pFtpNode->MkDir( ( char* )path.GetString( _param.charset, '/' ) );

	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	MutexLock lock( &mutex );
	p->busy = false;

	return 0;
}

int FSFtp::Delete( FSPath& path, int* err, FSCInfo* info )
{
	int nodeId = GetFreeNode( err, info );

	if ( nodeId < 0 ) { return nodeId; }

	Node* p = nodes + nodeId;

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );
		p->pFtpNode->Delete( ( char* )path.GetString( _param.charset, '/' ) );
		statCache.Del( path );

	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	MutexLock lock( &mutex );
	p->busy = false;


	return 0;
}

int FSFtp::RmDir  ( FSPath& path, int* err, FSCInfo* info )
{
	int nodeId = GetFreeNode( err, info );

	if ( nodeId < 0 ) { return nodeId; }

	Node* p = nodes + nodeId;

	try
	{
		CStopSetter stopSetter( p->pFtpNode.ptr(), info );
		FSPath par = path;
		par.Pop();
		p->pFtpNode->Cwd( ( char* )par.GetString( _param.charset, '/' ) ); //can use root /
		p->pFtpNode->RmDir( ( char* )path.GetString( _param.charset, '/' ) );
		statCache.Del( path );
	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	MutexLock lock( &mutex );
	p->busy = false;

	return 0;

}



int FSFtp::SetFileTime  ( FSPath& path, FSTime cTime, FSTime aTime, FSTime mTime, int* err, FSCInfo* info )
{
	return 0;
}


static int StringToMode( const char* s )
{
	int m = 0;

	if ( !*s ) { return m; }

	switch ( *s )
	{
		case 'd':
			m |= S_IFDIR;
			break;

		case 'c':
			m |= S_IFCHR;
			break;

		case 'b':
			m |= S_IFBLK;
			break;
#ifdef S_IFLNK

		case 'l':
			m |= S_IFLNK;
			break;
#endif

#ifdef S_IFSOCK

		case 's':
			m |= S_IFSOCK;
			break;
#endif

#ifdef S_IFIFO

		case 'p':
			m |= S_IFIFO;
			break;
#endif

		default:
			m |= S_IFREG;
	};

	s++;

	if ( !*s ) { return m; }


	//user
	if ( *s == 'r' ) { m |= S_IRUSR; }

	s++;

	if ( !*s ) { return m; }

	if ( *s == 'w' ) { m |= S_IWUSR; }

	s++;

	if ( !*s ) { return m; }

	switch ( *s )
	{
		case 'x':
			m |= S_IXUSR;
			break;

		case 's':
			m |= S_IXUSR | S_ISUID;
			break;

		case 'S':
			m |= S_ISUID;
			break;
	}

	s++;

	if ( !*s ) { return m; }

	//group
	if ( *s == 'r' ) { m |= S_IRGRP; }

	s++;

	if ( !*s ) { return m; }

	if ( *s == 'w' ) { m |= S_IWGRP; }

	s++;

	if ( !*s ) { return m; }

	switch ( *s )
	{
		case 'x':
			m |= S_IXGRP;
			break;

		case 's':
			m |= S_IXGRP | S_ISGID;
			break;

		case 'S':
			m |= S_ISGID;
			break;
	}

	s++;

	if ( !*s ) { return m; }

	//other
	if ( *s == 'r' ) { m |= S_IROTH; }

	s++;

	if ( !*s ) { return m; }

	if ( *s == 'w' ) { m |= S_IWOTH; }

	s++;

	if ( !*s ) { return m; }

	switch ( *s )
	{
		case 'x':
			m |= S_IXOTH;
			break;

		case 't':
			m |= S_IXOTH | S_ISVTX;
			break;

		case 'T':
			m |= S_ISVTX;
			break;
	}

	s++;

	if ( !*s ) { return m; }

	return m;
}


static time_t GetFtpFTime( const char* p1, const char* p2, const char* p3 )
{
	static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 0};
	int mon = 0;
	int i;

	for ( i = 0; months[i]; i++ )
		if ( !strcmp( months[i], p1 ) )
		{
			mon = i;
			break;
		}

	int day = atoi( p2 );

	char* t = ( char* )strchr( p3, ':' );
	int y = 0;
	int h = 0;
	int min = 0;

	if ( t )
	{
		time_t tim = time( 0 );

#if _MSC_VER > 1700
		struct tm st;

		if ( localtime_s( &st, &tim ) == 0 ) { y = st.tm_year; }

#elif defined( _WIN32 )
		struct tm* st = localtime( &tim );

		if ( st ) { y = st->tm_year; }

#else
		struct tm result;
		struct tm* st = localtime_r( &tim, &result );

		if ( st ) { y = st->tm_year; }

#endif

		*t = 0;
		t++;
		h = atoi( p3 );
		min = atoi( t );
	}
	else
	{
		y = atoi( p3 ) - 1900;
	}

	struct tm res;

	res.tm_sec = 0;

	res.tm_min = min;

	res.tm_hour = h;

	res.tm_mday = day;

	res.tm_mon = mon;

	res.tm_year = y;

	res.tm_wday = 0;

	res.tm_yday = 0;

	res.tm_isdst = 0;

	return mktime( &res );

}

inline unsigned GetUnsigned( const char* s )
{
	unsigned n = 0;

	for ( ; IsDigit( *s ); s++ )
	{
		n = n * 10 + ( *s - '0' );
	}

	return n;
}

static time_t GetFtpMSFTime( const char* p0, const char* p1 )
{
	/*
	05-10-00  12:48PM               205710 index.txt
	04-30-10  10:10AM       <DIR>          MSLFILES
	09-03-99  01:03PM                 2401 README.TXT
	*/
	int mon = GetUnsigned( p0 );

	if ( mon > 0 ) { mon--; }

	int day = GetUnsigned( p0 + 3 );
	int year = GetUnsigned( p0 + 6 );

	if ( year < 70 ) { year += 100; }

	if ( strlen( p1 ) != 7 ) { return 0; }

	int h = GetUnsigned( p1 );
	int min = GetUnsigned( p1 + 3 );

	if ( !strcmp( p1 + 5, "PM" ) ) { h += 12; }

	struct tm res;

	res.tm_sec = 0;

	res.tm_min = min;

	res.tm_hour = h;

	res.tm_mday = day;

	res.tm_mon = mon;

	res.tm_year = year;

	res.tm_wday = 0;

	res.tm_yday = 0;

	res.tm_isdst = 0;

	return mktime( &res );

}


int FSFtp::ReadDir( FSList* list, FSPath& _path, int* err, FSCInfo* info )
{
	clPtr<cstrhash<FSStat, char> > pSHash = new cstrhash<FSStat, char>;

	int r = ReadDir_int ( list,  pSHash.ptr(), _path, err, info );

	if ( r ) { return r; }

	statCache.PutStat( _path, pSHash );

	return r;
}

inline   long long ParzeFileSize( const char* s )
{
	long long size = 0;

	for ( ; *s >= '0' && *s <= '9'; s++ )
	{
		size = size * 10 + ( *s - '0' );
	}

	return size;
}

inline char* SkipSpace( char* s )
{
	while ( IsSpace( *s ) ) { s++; }

	return s;
}

inline char* ReadWord( char*& s )
{
	s = SkipSpace( s );
	char* ret = s;

	while ( *s && !IsSpace( *s ) ) { s++; }

	if ( *s )
	{
		*s = 0;
		s++;
	}

	return ret;
}


int FSFtp::ReadDir_int ( FSList* list, cstrhash<FSStat, char>* pSHash, FSPath& _path, int* err, FSCInfo* info )
{
	int nodeId = GetFreeNode( err, info );

	if ( nodeId < 0 ) { return nodeId; }

	Node* p = nodes + nodeId;

	try
	{
		FSPath path( _path );

		ccollect<std::string > ftp_list;

		CStopSetter stopSetter( p->pFtpNode.ptr(), info );

		p->pFtpNode->Cwd( ( char* )path.GetString( _param.charset, '/' ) );
		p->pFtpNode->Ls( ftp_list );

		if ( list ) { list->Clear(); }

		for ( int i = 0; i < ftp_list.count(); i++ )
		{
			char* s = (char*)ftp_list[i].data();

			if ( !s ) { continue; }


			/* unix
			-rw-r--r--    1 0          0                  36 Dec 24  2004 HEADER.html
			drwxrwsr-x   51 33         106              4096 Apr 25 17:14 pub

			-rw-r--r--   1 root     (?)      38500246 Jul  4 16:04 ftp_index.txt
			drwxr-xr-x   2 ftp      ftp           512 Jul  4 01:44 temp
			*/

			/* MS
			05-10-00  12:48PM               205710 index.txt
			04-30-10  10:10AM       <DIR>          MSLFILES
			09-03-99  01:03PM                 2401 README.TXT
			*/
			char* w0 = ReadWord( s );

			FSStat st;
			const char* fileName = "";

			if ( strlen( w0 ) == 8 ) // assume that - MS NN-NN-NN
			{
				char* w1 = ReadWord( s );
				char* w2 = ReadWord( s );

				st.mode = 0555;

				if ( !strcmp( w2, "<DIR>" ) )
				{
					st.mode |= S_IFDIR;
				}
				else
				{
					st.mode |= S_IFREG;
					st.size = ParzeFileSize( w2 );
				}

				s = SkipSpace( s );
				{
					int l = strlen( s );

					if ( l > 0 && s[l - 1] == '\r' ) { s[l - 1] = 0; }
				}

				st.m_LastWriteTime = GetFtpMSFTime( w0, w1 );
				st.m_LastAccessTime = st.m_LastWriteTime;
				st.m_ChangeTime = st.m_LastWriteTime;

				fileName = s;

			}
			else
			{
				st.mode = StringToMode( w0 );

				ReadWord( s ); //skip
				char* w2 = ReadWord( s );
				char* w3 = ReadWord( s );
				char* w4 = ReadWord( s );
				char* w5 = ReadWord( s );
				char* w6 = ReadWord( s );
				char* w7 = ReadWord( s );

				st.uid = uids.GetId( FSString( _param.charset, w2 ).GetUnicode() );
				st.gid = gids.GetId( FSString( _param.charset, w3 ).GetUnicode() );
				st.m_LastWriteTime = GetFtpFTime( w5, w6, w7 );
				st.m_ChangeTime = st.m_LastWriteTime;
				st.size = ParzeFileSize( w4 );

				s = SkipSpace( s );
				{
					int l = strlen( s );

					if ( l > 0 && s[l - 1] == '\r' ) { s[l - 1] = 0; }
				}
				fileName = s;
			}

			if ( !*fileName ) { continue; } //ignore empty file names

			if ( pSHash )
			{
				pSHash->get( fileName ) = st;
			}

			if ( list )
			{
				clPtr<FSNode> fsNode = new FSNode();
#if defined(__APPLE__)
				if ( _param.charset == CS_UTF8 )
				{
					std::string normname = normalize_utf8_NFC( fileName );
					fsNode->name = FSString( _param.charset, normname.data() );
				}
				else
				{
					fsNode->name = FSString( _param.charset, fileName );
				}
#else
				fsNode->name = FSString( _param.charset, fileName );
#endif
				fsNode->st = st;
				list->Append( fsNode );
			}
		}

	}
	catch ( int e )
	{
		MutexLock lock( &mutex );
		p->busy = false;

		if ( err ) { *err = e; }

		return ( e == -2 ) ? -2 : -1;
	}

	MutexLock lock( &mutex );
	p->busy = false;

	return 0;
}


int FSFtp::Stat   ( FSPath& path, FSStat* st, int* err, FSCInfo* info )
{
	if ( path.Count() <= 0 )
	{
		if ( err ) { *err = EFTP_INVALID_PARAMETER; }

		return -1;
	}

	int r = statCache.GetStat( path, st ); //0 - ok, 1 - not cache for it, -1 - not exist in cache

	if ( !r ) { return r; }

	if ( r < 0 )
	{
		if ( err ) { *err = EFTP_NOTEXIST; }

		return r;
	}

	clPtr<cstrhash<FSStat, char> > pSHash = new cstrhash<FSStat, char>;

	FSPath p1 = path;

	FSString name = *p1.GetItem( p1.Count() - 1 );

	p1.Pop();

	r = ReadDir_int ( 0,  pSHash.ptr(), p1, err, info );

	if ( r ) { return r; }

	FSStat* p = pSHash->exist( ( char* )name.Get( _param.charset ) );

	if ( p && st ) { *st = *p; }

	statCache.PutStat( p1, pSHash );

	if ( !p )
	{
		if ( err ) { *err = EFTP_NOTEXIST; }

		return -1;
	}

	return 0;
}

/*
int FSFtp::Symlink(FSPath &path, FSString &str, int *err, FSCInfo *info)
{
   if (err)
      *err = EFTP_NOTSUPPORTED;
   return -1;
}

int FSFtp::Seek(int fd, SEEK_FILE_MODE mode, seek_t pos, seek_t *pRet,  int *err, FSCInfo *info)
{
   if (err)
      *err = EFTP_NOTSUPPORTED;

   return -1;
}
*/

FSString FSFtp::Uri( FSPath& path )
{
	MutexLock lock( &mutex );

	std::string a;

	char port[0x100];
	Lsnprintf( port, sizeof( port ), ":%i", _param.port );

	FSString server( _param.server.c_str() );

	std::string PathStr( path.GetUtf8() );
	std::replace( PathStr.begin(), PathStr.end(), '\\', '/' );

	if ( !_param.anonymous )
	{
		FSString user( _param.user );
		FSString pass( _param.pass );
		if ( g_WcmConfig.systemStorePasswords && !pass.IsEmpty() )
		{
			a = std::string("ftp://") + user.GetUtf8() + ":" + pass.GetUtf8() + "@" + server.GetUtf8() + std::string(port) + PathStr;
		}
		else
		{
			a = std::string("ftp://") + user.GetUtf8() + "@" + server.GetUtf8() + std::string(port) + PathStr;
		}
	}
	else
	{
		a = std::string("ftp://") + server.GetUtf8() + std::string(port) + path.GetUtf8( '/' );
	}

	return FSString( CS_UTF8, a.data() );
}




unicode_t* FSFtp::GetUserName( int user, unicode_t buf[64] )
{
	const unicode_t* u = uids.GetName( user );
	unicode_t* s = buf;

	for ( int n = 63; n > 0 && *u; n--, u++, s++ )
	{
		*s = *u;
	}

	*s = 0;
	return buf;
};


unicode_t* FSFtp::GetGroupName( int group, unicode_t buf[64] )
{
	const unicode_t* g = gids.GetName( group );
	unicode_t* s = buf;

	for ( int n = 63; n > 0 && *g; n--, g++, s++ )
	{
		*s = *g;
	}

	*s = 0;

	return buf;
};



FSFtp::~FSFtp()
{
	dbg_printf( "\n!!!FSFtp destroyed\n" );
}



////////////////////////////////////////// FtpStatCache

FtpStatCache::Dir* FtpStatCache::GetParent( FSPath& path )
{
	if ( path.Count() <= 0 ) { return 0; }

	int n = 0;
	Dir* d = &root;

	for ( ; n < path.Count() - 1; n++ )
	{
		clPtr<Dir>* pd = d->dirs.exist( ( char* )path.GetItem( n )->Get( *pCharset ) );

		if ( !pd ) { return 0; }

		d = pd->ptr();

		if ( !d ) { return 0; }
	}

	return d;
}

int FtpStatCache::GetStat( FSPath path, FSStat* st )
{
	MutexLock lock( &mutex );

	Dir* d = GetParent( path );

	if ( !d ) { return 1; }

	if ( d->tim + CACHE_TIMEOUT < time( 0 ) )
	{
		d->statHash = 0;
		return 1;
	}

	if ( !d->statHash.ptr() ) { return 1; }

	FSStat* s = d->statHash->exist( ( char* )path.GetItem( path.Count() - 1 )->Get( *pCharset ) );

	if ( !s ) { return -1; }

	if ( st ) { *st = *s; }

	return 0;
}

void FtpStatCache::Del( FSPath path )
{
	MutexLock lock( &mutex );

	Dir* d = GetParent( path );

	if ( !d ) { return; }

	char* name = ( char* )path.GetItem( path.Count() - 1 )->Get( *pCharset );

	if ( d->statHash.ptr() ) { d->statHash->del( name ); }

	d->dirs.del( name );
}

void FtpStatCache::PutStat( FSPath path, clPtr<cstrhash<FSStat, char> > statHash )
{
	MutexLock lock( &mutex );

	int n = 0;
	Dir* d = &root;

	for ( ; n < path.Count(); n++ )
	{
		char* name = ( char* )path.GetItem( n )->Get( *pCharset );
		clPtr<Dir>* pd = &( d->dirs[name] );

		if ( !pd->ptr() ) { *pd = new Dir(); }

		d = pd->ptr();
	}

	d->statHash = statHash;
	d->tim = time( 0 );
}
