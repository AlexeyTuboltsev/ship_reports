#ifndef _RENDER_OVERLAY_H_
#define _RENDER_OVERLAY_H_

#include "ocpn_plugin.h"
#include <wx/dc.h>

class shipobs_pi;

// Render all stations using OpenGL
void RenderStationsGL(shipobs_pi *plugin, PlugIn_ViewPort *vp);

// Render all stations using wxDC (non-GL fallback)
void RenderStationsDC(shipobs_pi *plugin, wxDC &dc, PlugIn_ViewPort *vp);

// Invalidate the GL label texture cache (call when station list changes).
// Safe to call outside a GL frame — actual cleanup happens at next render.
void InvalidateLabelCache();

#endif  // _RENDER_OVERLAY_H_
