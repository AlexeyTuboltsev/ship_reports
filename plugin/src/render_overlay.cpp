#include "render_overlay.h"
#include "shipobs_pi.h"
#include "observation.h"

#include <cmath>
#include <map>
#include <vector>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/dcmemory.h>
#include <wx/font.h>
#include <wx/datetime.h>

// Marker size in pixels
static const int MARKER_SIZE = 8;

static float AgeOpacity(const wxDateTime &obs_time) {
  if (!obs_time.IsValid()) return 0.5f;
  wxTimeSpan age = wxDateTime::Now().ToUTC() - obs_time;
  double hours = age.GetSeconds().ToDouble() / 3600.0;
  if (hours < 0) hours = 0;
  if (hours > 24) return 0.15f;
  return static_cast<float>(1.0 - 0.7 * (hours / 24.0));
}

static void TypeColor(const wxString &type, float &r, float &g, float &b) {
  if (type == wxT("buoy")) {
    r = 1.0f;
    g = 0.85f;
    b = 0.0f;
  } else if (type == wxT("ship")) {
    r = 0.2f;
    g = 0.4f;
    b = 1.0f;
  } else if (type == wxT("shore")) {
    r = 0.0f;
    g = 0.8f;
    b = 0.2f;
  } else if (type == wxT("drifter")) {
    r = 0.0f;
    g = 0.9f;
    b = 0.9f;
  } else {
    r = 0.7f;
    g = 0.7f;
    b = 0.7f;
  }
}

static wxColour TypeWxColor(const wxString &type) {
  float r, g, b;
  TypeColor(type, r, g, b);
  return wxColour(static_cast<unsigned char>(r * 255),
                  static_cast<unsigned char>(g * 255),
                  static_cast<unsigned char>(b * 255));
}

static void DrawCircleGL(float cx, float cy, float radius, int segments = 16) {
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);
  for (int i = 0; i <= segments; i++) {
    float a = 2.0f * M_PI * i / segments;
    glVertex2f(cx + radius * cosf(a), cy + radius * sinf(a));
  }
  glEnd();
}

static void DrawTriangleGL(float cx, float cy, float size) {
  float h = size * 1.2f;
  glBegin(GL_TRIANGLES);
  glVertex2f(cx, cy - h);
  glVertex2f(cx - size, cy + h * 0.5f);
  glVertex2f(cx + size, cy + h * 0.5f);
  glEnd();
}

static void DrawSquareGL(float cx, float cy, float size) {
  glBegin(GL_QUADS);
  glVertex2f(cx - size, cy - size);
  glVertex2f(cx + size, cy - size);
  glVertex2f(cx + size, cy + size);
  glVertex2f(cx - size, cy + size);
  glEnd();
}

static void DrawDiamondGL(float cx, float cy, float size) {
  float s = size * 1.3f;
  glBegin(GL_QUADS);
  glVertex2f(cx, cy - s);
  glVertex2f(cx + s, cy);
  glVertex2f(cx, cy + s);
  glVertex2f(cx - s, cy);
  glEnd();
}

static void DrawMarkerGL(const wxString &type, float px, float py) {
  if (type == wxT("buoy")) {
    DrawCircleGL(px, py, MARKER_SIZE);
  } else if (type == wxT("ship")) {
    DrawTriangleGL(px, py, MARKER_SIZE);
  } else if (type == wxT("shore")) {
    DrawSquareGL(px, py, MARKER_SIZE * 0.8f);
  } else if (type == wxT("drifter")) {
    DrawDiamondGL(px, py, MARKER_SIZE * 0.7f);
  } else {
    DrawCircleGL(px, py, MARKER_SIZE * 0.6f);
  }
}

static void DrawWindBarbGL(float px, float py, double dir_deg, double spd_kt) {
  if (std::isnan(dir_deg) || std::isnan(spd_kt)) return;
  if (spd_kt < 0.5) return;

  float dir_rad = static_cast<float>(dir_deg * M_PI / 180.0);
  float shaft_len = 25.0f;
  float dx = sinf(dir_rad);
  float dy = -cosf(dir_rad);

  float ex = px + dx * shaft_len;
  float ey = py + dy * shaft_len;

  glBegin(GL_LINES);
  glVertex2f(px, py);
  glVertex2f(ex, ey);
  glEnd();

  int remaining = static_cast<int>(spd_kt + 2.5);
  float tick_spacing = 5.0f;
  float tick_pos = shaft_len;
  float nx = -dy;
  float ny = dx;
  float barb_len = 10.0f;

  while (remaining >= 50) {
    float bx = px + dx * tick_pos;
    float by = py + dy * tick_pos;
    float bx2 = bx + nx * barb_len;
    float by2 = by + ny * barb_len;
    float bx3 = px + dx * (tick_pos - tick_spacing);
    float by3 = py + dy * (tick_pos - tick_spacing);
    glBegin(GL_TRIANGLES);
    glVertex2f(bx, by);
    glVertex2f(bx2, by2);
    glVertex2f(bx3, by3);
    glEnd();
    tick_pos -= tick_spacing;
    remaining -= 50;
  }

  while (remaining >= 10) {
    float bx = px + dx * tick_pos;
    float by = py + dy * tick_pos;
    glBegin(GL_LINES);
    glVertex2f(bx, by);
    glVertex2f(bx + nx * barb_len, by + ny * barb_len);
    glEnd();
    tick_pos -= tick_spacing;
    remaining -= 10;
  }

  if (remaining >= 5) {
    float bx = px + dx * tick_pos;
    float by = py + dy * tick_pos;
    glBegin(GL_LINES);
    glVertex2f(bx, by);
    glVertex2f(bx + nx * barb_len * 0.5f, by + ny * barb_len * 0.5f);
    glEnd();
  }
}

struct LabelTex {
  GLuint id;
  int w, h;
};
static std::map<wxString, LabelTex> s_label_cache;
static bool s_cache_dirty = false;

void InvalidateLabelCache() { s_cache_dirty = true; }

static void ClearLabelCache() {
  for (auto &kv : s_label_cache) glDeleteTextures(1, &kv.second.id);
  s_label_cache.clear();
  s_cache_dirty = false;
}

static const LabelTex *GetOrCreateLabelTex(const wxString &text) {
  auto it = s_label_cache.find(text);
  if (it != s_label_cache.end()) return &it->second;

  wxFont font(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

  wxCoord tw = 1, th = 1;
  {
    wxBitmap tmp(1, 1);
    wxMemoryDC mdc(tmp);
    mdc.SetFont(font);
    mdc.GetTextExtent(text, &tw, &th);
    tw += 2;
    th += 2;
  }

  wxBitmap bmp(tw, th);
  {
    wxMemoryDC mdc(bmp);
    mdc.SetFont(font);
    mdc.SetBackground(*wxBLACK_BRUSH);
    mdc.Clear();
    mdc.SetTextForeground(*wxWHITE);
    mdc.DrawText(text, 1, 1);
  }

  wxImage img = bmp.ConvertToImage();
  std::vector<unsigned char> px(tw * th * 4);
  for (int y = 0; y < th; y++) {
    for (int x = 0; x < tw; x++) {
      unsigned char a = img.GetRed(x, y);
      px[4 * (y * tw + x) + 0] = 255;
      px[4 * (y * tw + x) + 1] = 255;
      px[4 * (y * tw + x) + 2] = 255;
      px[4 * (y * tw + x) + 3] = a;
    }
  }

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               px.data());
  glBindTexture(GL_TEXTURE_2D, 0);

  LabelTex lt{tex, (int)tw, (int)th};
  s_label_cache[text] = lt;
  return &s_label_cache[text];
}

static void DrawLabelGL(const wxString &text, float px, float py, float alpha) {
  const LabelTex *lt = GetOrCreateLabelTex(text);
  if (!lt) return;

  float x = px + MARKER_SIZE + 3;
  float y = py - lt->h / 2.0f;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, lt->id);
  glColor4f(0.3f, 0.3f, 0.3f, alpha);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 0);
  glVertex2f(x, y);
  glTexCoord2f(1, 0);
  glVertex2f(x + lt->w, y);
  glTexCoord2f(1, 1);
  glVertex2f(x + lt->w, y + lt->h);
  glTexCoord2f(0, 1);
  glVertex2f(x, y + lt->h);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

void RenderStationsGL(shipobs_pi *plugin, PlugIn_ViewPort *vp) {
  const ObservationList &stations = plugin->GetStations();
  if (stations.empty()) return;

  if (s_cache_dirty) ClearLabelCache();

  bool show_barbs = plugin->GetShowWindBarbs();
  bool show_labels = plugin->GetShowLabels();

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glLineWidth(1.5f);

  for (size_t i = 0; i < stations.size(); i++) {
    const ObservationStation &st = stations[i];
    if (std::isnan(st.lat) || std::isnan(st.lon)) continue;

    wxPoint pt;
    GetCanvasPixLL(vp, &pt, st.lat, st.lon);
    float px = static_cast<float>(pt.x);
    float py = static_cast<float>(pt.y);

    if (pt.x < -50 || pt.x > vp->pix_width + 50 || pt.y < -50 ||
        pt.y > vp->pix_height + 50)
      continue;

    float opacity = AgeOpacity(st.time);
    float r, g, b;
    TypeColor(st.type, r, g, b);

    if (plugin->IsStationHighlighted(st.id)) {
      glColor4f(243 / 255.0f, 229 / 255.0f, 47 / 255.0f, 0.75f);
      DrawCircleGL(px, py, MARKER_SIZE + 7);
    }

    glColor4f(r, g, b, opacity);
    DrawMarkerGL(st.type, px, py);

    glColor4f(0, 0, 0, opacity);
    glLineWidth(1.0f);

    if (show_barbs) {
      glColor4f(0, 0, 0, opacity);
      glLineWidth(1.5f);
      DrawWindBarbGL(px, py, st.wind_dir, st.wind_spd);
    }

    if (show_labels && !st.id.IsEmpty()) {
      DrawLabelGL(st.id, px, py, opacity);
    }
  }

  glDisable(GL_BLEND);
}

static void DrawMarkerDC(wxDC &dc, const wxString &type, int px, int py) {
  if (type == wxT("buoy")) {
    dc.DrawCircle(px, py, MARKER_SIZE);
  } else if (type == wxT("ship")) {
    wxPoint pts[3];
    int h = MARKER_SIZE + 2;
    pts[0] = wxPoint(px, py - h);
    pts[1] = wxPoint(px - MARKER_SIZE, py + h / 2);
    pts[2] = wxPoint(px + MARKER_SIZE, py + h / 2);
    dc.DrawPolygon(3, pts);
  } else if (type == wxT("shore")) {
    int s = MARKER_SIZE;
    dc.DrawRectangle(px - s, py - s, s * 2, s * 2);
  } else if (type == wxT("drifter")) {
    wxPoint pts[4];
    int s = MARKER_SIZE;
    pts[0] = wxPoint(px, py - s);
    pts[1] = wxPoint(px + s, py);
    pts[2] = wxPoint(px, py + s);
    pts[3] = wxPoint(px - s, py);
    dc.DrawPolygon(4, pts);
  } else {
    dc.DrawCircle(px, py, MARKER_SIZE - 2);
  }
}

void RenderStationsDC(shipobs_pi *plugin, wxDC &dc, PlugIn_ViewPort *vp) {
  const ObservationList &stations = plugin->GetStations();
  if (stations.empty()) return;

  bool show_labels = plugin->GetShowLabels();

  dc.SetTextForeground(wxColour(77, 77, 77));
  wxFont font(8, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
  dc.SetFont(font);

  for (size_t i = 0; i < stations.size(); i++) {
    const ObservationStation &st = stations[i];
    if (std::isnan(st.lat) || std::isnan(st.lon)) continue;

    wxPoint pt;
    GetCanvasPixLL(vp, &pt, st.lat, st.lon);

    if (pt.x < -50 || pt.x > vp->pix_width + 50 || pt.y < -50 ||
        pt.y > vp->pix_height + 50)
      continue;

    float opacity = AgeOpacity(st.time);
    wxColour col = TypeWxColor(st.type);
    unsigned char alpha = static_cast<unsigned char>(opacity * 255);
    wxColour blended((col.Red() * alpha + 255 * (255 - alpha)) / 255,
                     (col.Green() * alpha + 255 * (255 - alpha)) / 255,
                     (col.Blue() * alpha + 255 * (255 - alpha)) / 255);

    if (plugin->IsStationHighlighted(st.id)) {
      dc.SetBrush(wxBrush(wxColour(243, 229, 47)));
      dc.SetPen(wxPen(wxColour(243, 229, 47), 1));
      dc.DrawCircle(pt.x, pt.y, MARKER_SIZE + 7);
    }

    dc.SetBrush(wxBrush(blended));
    dc.SetPen(wxPen(*wxBLACK, 1));

    DrawMarkerDC(dc, st.type, pt.x, pt.y);

    if (show_labels && !st.id.IsEmpty()) {
      dc.DrawText(st.id, pt.x + MARKER_SIZE + 3, pt.y - 5);
    }
  }
}
