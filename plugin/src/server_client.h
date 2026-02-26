#ifndef _SERVER_CLIENT_H_
#define _SERVER_CLIENT_H_

#include "observation.h"
#include <wx/string.h>

// Fetch observations from the server within the given bounding box.
bool FetchObservations(const wxString &server_url, double lat_min,
                       double lat_max, double lon_min, double lon_max,
                       const wxString &max_age, const wxString &types,
                       ObservationList &out, wxString &error_msg);

#endif  // _SERVER_CLIENT_H_
