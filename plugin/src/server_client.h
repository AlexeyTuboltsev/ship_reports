#ifndef _SERVER_CLIENT_H_
#define _SERVER_CLIENT_H_

#include "observation.h"
#include <wx/string.h>

// Fetch observations from the server within the given bounding box.
// Parameters:
//   server_url  - Base URL, e.g. "http://localhost:8080"
//   lat_min/max, lon_min/max - Bounding box
//   max_age     - e.g. "6h", "12h", "24h"
//   types       - Comma-separated, e.g. "ship,buoy,shore"
//   out         - Filled with parsed observations on success
// Returns true on success, false on HTTP or parse error.
bool FetchObservations(const wxString &server_url,
                       double lat_min, double lat_max,
                       double lon_min, double lon_max,
                       const wxString &max_age,
                       const wxString &types,
                       ObservationList &out,
                       wxString &error_msg);

#endif // _SERVER_CLIENT_H_
