// RenderDirect3D stub: the Direct3D 9 renderer needs d3dx9math from the retired DirectX SDK.
// Every request for it is served by the OpenGL renderer instead.

#include "stdafx.h"
#include "video/RenderDirect3D.h"
#include "video/RenderOpenGL.h"

IRender* renderCreateDirect3D()
{
	return renderCreateOpenGL();
}
