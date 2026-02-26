#include "server_client.h"
#include "obs_parser.h"
#include "url_builder.h"

#include <curl/curl.h>
#include <wx/intl.h>
#include <wx/log.h>

static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb,
                                void *userdata) {
  std::string *buf = static_cast<std::string *>(userdata);
  buf->append(ptr, size * nmemb);
  return size * nmemb;
}

bool FetchObservations(const wxString &server_url, double lat_min,
                       double lat_max, double lon_min, double lon_max,
                       const wxString &max_age, const wxString &types,
                       ObservationList &out, wxString &error_msg) {
  BBox bb = ClampBbox(lat_min, lat_max, lon_min, lon_max);

  std::string s_max_age = std::string(max_age.mb_str(wxConvUTF8));
  std::string s_types = std::string(types.mb_str(wxConvUTF8));

  std::string url = std::string(server_url.mb_str(wxConvUTF8)) +
                    "/api/v1/observations?lat_min=" + FmtDbl(bb.lat_min) +
                    "&lat_max=" + FmtDbl(bb.lat_max) +
                    "&lon_min=" + FmtDbl(bb.lon_min) +
                    "&lon_max=" + FmtDbl(bb.lon_max) + "&max_age=" + s_max_age +
                    "&types=" + s_types;

  wxLogMessage(
      "ShipObs: fetch  lat=[%.4f, %.4f]  lon=[%.4f, %.4f]  age=%s  types=%s",
      bb.lat_min, bb.lat_max, bb.lon_min, bb.lon_max, s_max_age.c_str(),
      s_types.c_str());
  wxLogMessage("ShipObs: URL: %s", url.c_str());

  CURL *curl = curl_easy_init();
  if (!curl) {
    error_msg = _("Failed to initialize HTTP client");
    wxLogError("ShipObs: failed to initialize curl");
    return false;
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    error_msg =
        wxString::Format(_("Failed to connect to server: %s"), server_url);
    wxLogError("ShipObs: fetch failed: %s", curl_easy_strerror(res));
    return false;
  }

  if (response.empty()) {
    error_msg = _("Empty response from server");
    wxLogError("ShipObs: HTTP %ld, empty response", http_code);
    return false;
  }

  wxLogMessage("ShipObs: HTTP %ld, %zu bytes", http_code, response.size());

  if (http_code != 200) {
    std::string excerpt = response.substr(0, 300);
    wxLogError("ShipObs: server error body: %s", excerpt.c_str());
    error_msg = wxString::Format(_("Server returned HTTP %ld"), http_code);
    return false;
  }

  wxString json = wxString::FromUTF8(response.c_str(), response.size());
  return ParseObservations(json, out, error_msg);
}
