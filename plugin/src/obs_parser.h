#ifndef _OBS_PARSER_H_
#define _OBS_PARSER_H_

#include "observation.h"
#include <wx/string.h>

// Parse a JSON response string from the shipobs server into an ObservationList.
// Stations missing required fields (id, lat, lon, time) are silently dropped.
// Returns false (and sets error_msg) only on a structural JSON error.
bool ParseObservations(const wxString &json, ObservationList &out,
                       wxString &error_msg);

#endif // _OBS_PARSER_H_
