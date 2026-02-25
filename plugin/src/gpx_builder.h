#ifndef _GPX_BUILDER_H_
#define _GPX_BUILDER_H_

#include "observation.h"
#include <wx/datetime.h>
#include <wx/string.h>

// Build a GPX document string from a list of stations.
// fetched_at: timestamp of the fetch (appended to each waypoint description).
// Stations with NaN lat/lon are skipped.
wxString BuildGPXString(const wxDateTime &fetched_at,
                        const ObservationList &stations);

#endif // _GPX_BUILDER_H_
