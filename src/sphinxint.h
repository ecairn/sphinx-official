//
// $Id$
//

//
// Copyright (c) 2001-2010, Andrew Aksyonoff
// Copyright (c) 2008-2010, Sphinx Technologies Inc
// All rights reserved
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License. You should have
// received a copy of the GPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//

#ifndef _sphinxint_
#define _sphinxint_

#include "sphinx.h"
#include "sphinxfilter.h"

#include <sys/stat.h>
#include <fcntl.h>

//////////////////////////////////////////////////////////////////////////

#ifdef O_BINARY
#define SPH_O_BINARY O_BINARY
#else
#define SPH_O_BINARY 0
#endif

#define SPH_O_READ	( O_RDONLY | SPH_O_BINARY )
#define SPH_O_NEW	( O_CREAT | O_RDWR | O_TRUNC | SPH_O_BINARY )

/// file writer with write buffering and int encoder
class CSphWriter : ISphNoncopyable
{
public:
					CSphWriter ();
	virtual			~CSphWriter ();

	void			SetBufferSize ( int iBufferSize );	///< tune write cache size; must be called before OpenFile() or SetFile()

	bool			OpenFile ( const CSphString & sName, CSphString & sErrorBuffer );
	void			SetFile ( int iFD, SphOffset_t * pSharedOffset );
	void			CloseFile ( bool bTruncate = false );	///< note: calls Flush(), ie. IsError() might get true after this call

	void			PutByte ( int uValue );
	void			PutBytes ( const void * pData, int iSize );
	void			PutDword ( DWORD uValue ) { PutBytes ( &uValue, sizeof(DWORD) ); }
	void			PutOffset ( SphOffset_t uValue ) { PutBytes ( &uValue, sizeof(SphOffset_t) ); }
	void			PutString ( const char * szString );

	void			SeekTo ( SphOffset_t pos ); ///< seeking inside the buffer will truncate it

#if USE_64BIT
	void			PutDocid ( SphDocID_t uValue ) { PutOffset ( uValue ); }
#else
	void			PutDocid ( SphDocID_t uValue ) { PutDword ( uValue ); }
#endif

	void			ZipInt ( DWORD uValue );
	void			ZipOffset ( SphOffset_t uValue );
	void			ZipOffsets ( CSphVector<SphOffset_t> * pData );

	bool			IsError () const	{ return m_bError; }
	SphOffset_t		GetPos () const		{ return m_iPos; }

protected:
	CSphString		m_sName;
	SphOffset_t		m_iPos;
	SphOffset_t		m_iWritten;

	int				m_iFD;
	int				m_iPoolUsed;
	BYTE *			m_pBuffer;
	BYTE *			m_pPool;
	bool			m_bOwnFile;
	SphOffset_t	*	m_pSharedOffset;
	int				m_iBufferSize;

	bool			m_bError;
	CSphString *	m_pError;

	virtual void	Flush ();
};

/// forward ref
class CSphAutofile;

/// file reader with read buffering and int decoder
class CSphReader
{
public:
	CSphReader ( BYTE * pBuf=NULL, int iSize=0 );
	virtual		~CSphReader ();

	void		SetBuffers ( int iReadBuffer, int iReadUnhinted );
	void		SetFile ( int iFD, const char * sFilename );
	void		SetFile ( const CSphAutofile & tFile );
	void		Reset ();
	void		SeekTo ( SphOffset_t iPos, int iSizeHint );

	void		SkipBytes ( int iCount );
	SphOffset_t	GetPos () const { return m_iPos+m_iBuffPos; }

	void		GetBytes ( void * pData, int iSize );
	int			GetBytesZerocopy ( const BYTE ** ppData, int iMax ); ///< zerocopy method; returns actual length present in buffer (upto iMax)

	int			GetByte ();
	DWORD		GetDword ();
	SphOffset_t	GetOffset ();
	CSphString	GetString ();
	int			GetLine ( char * sBuffer, int iMaxLen );

	DWORD		UnzipInt ();
	SphOffset_t	UnzipOffset ();

	SphOffset_t				Tell () const				{ return m_iPos + m_iBuffPos; }
	bool					GetErrorFlag () const		{ return m_bError; }
	const CSphString &		GetErrorMessage () const	{ return m_sError; }
	const CSphString &		GetFilename() const			{ return m_sFilename; }

#if USE_64BIT
	SphDocID_t	GetDocid ()		{ return GetOffset(); }
	SphDocID_t	UnzipDocid ()	{ return UnzipOffset(); }
	SphWordID_t	UnzipWordid ()	{ return UnzipOffset(); }
#else
	SphDocID_t	GetDocid ()		{ return GetDword(); }
	SphDocID_t	UnzipDocid ()	{ return UnzipInt(); }
	SphWordID_t	UnzipWordid ()	{ return UnzipInt(); }
#endif

	const CSphReader &	operator = ( const CSphReader & rhs );

protected:

	int			m_iFD;
	SphOffset_t	m_iPos;

	int			m_iBuffPos;
	int			m_iBuffUsed;
	BYTE *		m_pBuff;
	int			m_iSizeHint;	///< how much do we expect to read

	int			m_iBufSize;
	bool		m_bBufOwned;
	int			m_iReadUnhinted;

	bool		m_bError;
	CSphString	m_sError;
	CSphString	m_sFilename;

private:
	void		UpdateCache ();
};

/// scoped reader
class CSphAutoreader : public CSphReader
{
public:
				CSphAutoreader ( BYTE * pBuf=NULL, int iSize=0 ) : CSphReader ( pBuf, iSize ) {}
				~CSphAutoreader ();

	bool		Open ( const CSphString & sFilename, CSphString & sError );
	void		Close ();
	SphOffset_t	GetFilesize ();

public:
	// added for DebugCheck()
	int			GetFD () { return m_iFD; }
};

//////////////////////////////////////////////////////////////////////////

/// per-query search context
/// everything that index needs to compute/create to process the query
class CSphQueryContext
{
public:
	// searching-only, per-query
	int							m_iWeights;						///< search query field weights count
	int							m_dWeights [ SPH_MAX_FIELDS ];	///< search query field weights

	bool						m_bEarlyLookup;			///< whether early attr value lookup is needed
	bool						m_bLateLookup;			///< whether late attr value lookup is needed

	ISphFilter *				m_pFilter;
	ISphFilter *				m_pWeightFilter;

	struct CalcItem_t
	{
		CSphAttrLocator			m_tLoc;					///< result locator
		DWORD					m_uType;				///< result type
		ISphExpr *				m_pExpr;				///< evaluator (non-owned)
	};
	CSphVector<CalcItem_t>		m_dEarlyCalc;			///< early-calc evaluators
	CSphVector<CalcItem_t>		m_dLateCalc;			///< late-calc evaluators

	const CSphVector<CSphAttrOverride> *	m_pOverrides;		///< overridden attribute values
	CSphVector<CSphAttrLocator>				m_dOverrideIn;
	CSphVector<CSphAttrLocator>				m_dOverrideOut;

	void *						m_pIndexData;			///< backend specific data

public:
	CSphQueryContext ();
	~CSphQueryContext ();

	void						BindWeights ( const CSphQuery * pQuery, const CSphSchema & tSchema );
	bool						SetupCalc ( CSphQueryResult * pResult, const CSphSchema & tInSchema, const CSphSchema & tSchema, const DWORD * pMvaPool );
	bool						CreateFilters ( bool bFullscan, const CSphVector<CSphFilterSettings> * pdFilters, const CSphSchema & tSchema, const DWORD * pMvaPool, CSphString & sError );
	bool						SetupOverrides ( const CSphQuery * pQuery, CSphQueryResult * pResult, const CSphSchema & tIndexSchema );

	void						EarlyCalc ( CSphMatch & tMatch ) const;
	void						LateCalc ( CSphMatch & tMatch ) const;
};

bool sphWriteThrottled ( int iFD, const void * pBuf, int64_t iCount, const char * sName, CSphString & sError );

void SafeClose ( int & iFD );

//////////////////////////////////////////////////////////////////////////

/// memory tracker


namespace Memory
{
	enum Category_e
	{
		SPH_MEM_CORE,

		SPH_MEM_IDX_DISK,
		SPH_MEM_IDX_RT,
		SPH_MEM_IDX_RT_ACCUM,

		SPH_MEM_BINLOG,

		SPH_MEM_HANDLE_NONSQL,
		SPH_MEM_HANDLE_SQL,

		SPH_MEM_SEARCH_NONSQL,
		SPH_MEM_QUERY_NONSQL,
		SPH_MEM_INSERT_SQL,
		SPH_MEM_SELECT_SQL,
		SPH_MEM_DELETE_SQL,
		SPH_MEM_COMMIT_SET_SQL,
		SPH_MEM_COMMIT_START_T_SQL,
		SPH_MEM_COMMIT_SQL,

		SPH_MEM_IDX_DISK_MULTY_QUERY,
		SPH_MEM_IDX_DISK_MULTY_QUERY_EX,
		SPH_MEM_IDX_RT_MULTY_QUERY,

		SPH_MEM_TOTAL
	};
}

#if SPH_ALLOCS_PROFILER

void sphMemStatPush ( Memory::Category_e eCategory );
void sphMemStatPop ( Memory::Category_e eCategory );

// memory tracker
struct MemTracker_c : ISphNoncopyable
{
	const Memory::Category_e m_eCategory; ///< category

	/// ctor
	explicit MemTracker_c ( Memory::Category_e eCategory )
		: m_eCategory ( eCategory )
	{
		sphMemStatPush ( m_eCategory );
	}

	/// dtor
	~MemTracker_c ()
	{
		sphMemStatPop ( m_eCategory );
	}
};

#define MEMORY(name) MemTracker_c tracker_##__LINE__(Memory::name);

#else // SPH_ALLOCS_PROFILER 0

#define MEMORY(name)

#endif // if SPH_ALLOCS_PROFILER


#endif // _sphinxint_

//
// $Id$
//
