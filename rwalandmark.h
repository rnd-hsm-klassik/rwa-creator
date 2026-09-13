/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * License: MIT
 *
 * rwalandmark.h
 *
 * A landmark: a named, described position on the map, recorded from the hero
 * (an RTK headtracker GPS fix when the hero follows it, or wherever the hero
 * was dragged). Authoring aid only: stored in the .rwa as a <landmark> element,
 * ignored by RWA Player ignores.
 * Qt-free like the rest of the game model, so the headless importer builds too.
 */

#ifndef RWALANDMARK_H
#define RWALANDMARK_H

#include "rwalocation1.h"

#define RWALOCATIONTYPE_LANDMARK 9

class RwaLandmark : public RwaLocation1
{
public:
    RwaLandmark(const std::string &name, const std::vector<double> &coordinates);

    std::string description;
    std::string captured;      ///< ISO 8601 local time of recording, empty if unknown
    std::string source;        ///< "rtk" (live headtracker fix) or "hand" (hero placed by hand / OSC)
    int carrSoln = 0;          ///< receiver carrier solution at capture: 0 none, 1 RTK float, 2 RTK fixed
    unsigned int hAccMm = 0;   ///< receiver horizontal accuracy estimate at capture, 0 = unknown

    void moveMyChildren(double dx, double dy) override;
};

#endif // RWALANDMARK_H
