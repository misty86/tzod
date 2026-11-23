// dxerr.h shim: the DirectX SDK error helpers are gone from the Windows SDK;
// the game only used DXTRACE_ERR, which passes the HRESULT through.
#pragma once
#define DXTRACE_ERR(str, hr) (hr)
#define DXTRACE_ERR_MSGBOX(str, hr) (hr)
#define DXTRACE_MSG(str) ((void)0)
