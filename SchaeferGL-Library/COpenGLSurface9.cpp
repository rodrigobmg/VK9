//========= Copyright Valve Corporation, All rights reserved. ============//
//                       TOGL CODE LICENSE
//
//  Copyright 2011-2014 Valve Corporation
//  All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
// dxabstract.h
//
//==================================================================================================

/*
 * Code from ToGL has been modified to fit the design.
 */
 
#include "COpenGLSurface9.h"
#include "COpenGLDevice9.h"

COpenGLSurface9::COpenGLSurface9()
{
	m_refcount[0] = 1;
	m_refcount[1] = 0;
	m_mark = (IUNKNOWN_ALLOC_SPEW_MARK_ALL != 0);	// either all are marked, or only the ones that have SetMark(true) called on them
}

COpenGLSurface9::~COpenGLSurface9()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );

	// not much to do here, but good to verify that these things are being freed (and they are)
	//GLMPRINTF(("-A-  ~IDirect3DSurface9 - signpost"));

	if (m_device)
	{
		GLMPRINTF(("-A-  ~IDirect3DSurface9 - taking real delete path on %08x device %08x", this, m_device));
		m_device->ReleasedSurface( this );

		memset( &m_desc, 0, sizeof(m_desc) );

		if (m_restype != 0)	// signal that we are a surface that owns this tex (render target)
		{
			if (m_tex)
			{
				GLMPRINTF(("-A- ~IDirect3DSurface9 deleted '%s' @ %08x (GLM %08x) %s",m_tex->m_layout->m_layoutSummary, this, m_tex, m_tex->m_debugLabel ? m_tex->m_debugLabel : "" ));

				m_device->ReleasedCGLMTex( m_tex );

				m_tex->m_ctx->DelTex( m_tex );
				m_tex = NULL;
			}
			else
			{
				GLMPRINTF(( "-A- ~IDirect3DSurface9 : whoops, no tex to delete here ?" ));
			}		
		}
		else
		{
			m_tex = NULL;	// we are just a view on the tex, we don't own the tex, do not delete it
		}

		m_face = m_mip = 0;
		
		m_device = NULL;
	}
	else
	{
		GLMPRINTF(("-A-  ~IDirect3DSurface9 - taking strange delete path on %08x device %08x", this, m_device));
	}	
}

ULONG STDMETHODCALLTYPE COpenGLSurface9::AddRef(void)
{
	return this->AddRef(0);
}

HRESULT STDMETHODCALLTYPE COpenGLSurface9::QueryInterface(REFIID riid,void  **ppv)
{
	return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE COpenGLSurface9::Release(void)
{
	return this->Release(0);
}

ULONG STDMETHODCALLTYPE COpenGLSurface9::AddRef(int which, char *comment)
{
	Assert( which >= 0 );
	Assert( which < 2 );
	m_refcount[which]++;
		
	#if IUNKNOWN_ALLOC_SPEW
		if (m_mark)
		{
			GLMPRINTF(("-A- IUAddRef  (%08x,%d) refc -> (%d,%d) [%s]",this,which,m_refcount[0],m_refcount[1],comment?comment:"..."))	;
			if (!comment)
			{
				GLMPRINTF((""))	;	// place to hang a breakpoint
			}
		}
	#endif	

	return m_refcount[0];
}

//ULONG STDMETHODCALLTYPE	COpenGLSurface9::Release(int which, char *comment)
//{
//	Assert( which >= 0 );
//	Assert( which < 2 );
//		
//	//int oldrefcs[2] = { m_refcount[0], m_refcount[1] };
//	bool deleting = false;
//		
//	m_refcount[which]--;
//	if ( (!m_refcount[0]) && (!m_refcount[1]) )
//	{
//		deleting = true;
//	}
//		
//	#if IUNKNOWN_ALLOC_SPEW
//		if (m_mark)
//		{
//			GLMPRINTF(("-A- IURelease (%08x,%d) refc -> (%d,%d) [%s] %s",this,which,m_refcount[0],m_refcount[1],comment?comment:"...",deleting?"->DELETING":""));
//			if (!comment)
//			{
//				GLMPRINTF((""))	;	// place to hang a breakpoint
//			}
//		}
//	#endif
//
//	if (deleting)
//	{
//		if (m_mark)
//		{
//			GLMPRINTF((""))	;		// place to hang a breakpoint
//		}
//		delete this;
//		return 0;
//	}
//	else
//	{
//		return m_refcount[0];
//	}
//}

HRESULT STDMETHODCALLTYPE COpenGLSurface9::FreePrivateData(REFGUID refguid)
{
	return E_NOTIMPL;
}

DWORD STDMETHODCALLTYPE COpenGLSurface9::GetPriority()
{
	return 1;
}

HRESULT STDMETHODCALLTYPE COpenGLSurface9::GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData)
{
	return E_NOTIMPL;
}

D3DRESOURCETYPE STDMETHODCALLTYPE COpenGLSurface9::GetType()
{
	return D3DRTYPE_SURFACE;
}

void STDMETHODCALLTYPE COpenGLSurface9::PreLoad()
{
	return; 
}

DWORD STDMETHODCALLTYPE COpenGLSurface9::SetPriority(DWORD PriorityNew)
{
	return 1;
}

HRESULT STDMETHODCALLTYPE COpenGLSurface9::SetPrivateData(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE COpenGLSurface9::GetContainer(REFIID riid, void** ppContainer)
{
	return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE COpenGLSurface9::GetDC(HDC* phdc)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE COpenGLSurface9::GetDesc(D3DSURFACE_DESC* pDesc)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	*pDesc = m_desc;
	return S_OK;	
}

HRESULT STDMETHODCALLTYPE COpenGLSurface9::LockRect(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags)
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex	= this->m_tex;
	lockreq.m_face	= this->m_face;
	lockreq.m_mip	= this->m_mip;

	lockreq.m_region.xmin = pRect->left;
	lockreq.m_region.ymin = pRect->top;
	lockreq.m_region.zmin = 0;
	lockreq.m_region.xmax = pRect->right;
	lockreq.m_region.ymax = pRect->bottom;
	lockreq.m_region.zmax = 1;
	
	if ((Flags & (D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK)) == (D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK) )
	{
		// smells like readback, force texel readout
		lockreq.m_readback = true;
	}
	
	char	*lockAddress;
	int		yStride;
	int		zStride;
	
	lockreq.m_tex->Lock( &lockreq, &lockAddress, &yStride, &zStride );

	pLockedRect->Pitch = yStride;
	pLockedRect->pBits = lockAddress;
	
	return S_OK;	
}


HRESULT STDMETHODCALLTYPE COpenGLSurface9::ReleaseDC(HDC hdc)
{
	return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE COpenGLSurface9::UnlockRect()
{
	GL_BATCH_PERF_CALL_TIMER;
	GL_PUBLIC_ENTRYPOINT_CHECKS( m_device );
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex	= this->m_tex;
	lockreq.m_face	= this->m_face;
	lockreq.m_mip	= this->m_mip;

	lockreq.m_tex->Unlock( &lockreq );

	return S_OK;	
}

ULONG STDMETHODCALLTYPE	COpenGLSurface9::Release( int which, char *comment)
{
	Assert( which >= 0 );
	Assert( which < 2 );
		
	//int oldrefcs[2] = { m_refcount[0], m_refcount[1] };
	bool deleting = false;
		
	m_refcount[which]--;
	if ( (!m_refcount[0]) && (!m_refcount[1]) )
	{
		deleting = true;
	}
		
	#if IUNKNOWN_ALLOC_SPEW
		if (m_mark)
		{
			GLMPRINTF(("-A- IURelease (%08x,%d) refc -> (%d,%d) [%s] %s",this,which,m_refcount[0],m_refcount[1],comment?comment:"...",deleting?"->DELETING":""));
			if (!comment)
			{
				GLMPRINTF((""))	;	// place to hang a breakpoint
			}
		}
	#endif

	if (deleting)
	{
		if (m_mark)
		{
			GLMPRINTF((""))	;		// place to hang a breakpoint
		}
		delete this;
		return 0;
	}
	else
	{
		return m_refcount[0];
	}
}