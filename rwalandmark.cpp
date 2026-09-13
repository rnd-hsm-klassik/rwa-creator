#include "rwalandmark.h"

RwaLandmark::RwaLandmark(const std::string &name, const std::vector<double> &coordinates)
{
    setObjectName(name);
    setCoordinates(coordinates);
    setLocationType(RWALOCATIONTYPE_LANDMARK);
}

void RwaLandmark::moveMyChildren(double dx, double dy)
{
    (void)dx;
    (void)dy;
}
