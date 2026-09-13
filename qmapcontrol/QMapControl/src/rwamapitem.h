#ifndef RWAMAPITEM_H
#define RWAMAPITEM_H

#include "point.h"

namespace qmapcontrol
{
    class RwaMapItem : public QmapPoint
    {
    public:
        RwaMapItem(QPointF position, RwaLocation1 *rwaItem, int rwaType, QPixmap *pixmap, int channel = -1);

        RwaLocation1 *getRwaItem() const;
        int getRwaType() const;
        int getChannel() const;

    private:
        RwaLocation1 *rwaItem;
        int rwaType = 0;
        int channel = -1;
    };

    /** 
     * A dotted line between two coordinates: the path of a moving asset from its start
     * position to its target. Derived from QmapPoint rather than LineString because
     * Layer::clearGeometries() only deletes "Point" geometries (and removeGeometry()
     * casts to QmapPoint blindly).
     */
    class RwaTrajectory : public QmapPoint
    {
    public:
        RwaTrajectory(RwaLocation1 *rwaItem, const QPen &pen);

        RwaLocation1 *getRwaItem() const;
        void setEndpoints(const QPointF &from, const QPointF &to);
        void setTrajectoryPen(const QPen &pen);

    protected:
        void draw(QPainter *painter, const MapAdapter *mapadapter, const QRect &viewport, const QPoint offset) override;
        bool Touches(QmapPoint *geom, const MapAdapter *mapadapter) override;

    private:
        RwaLocation1 *rwaItem;
        QPointF fromPoint;
        QPointF toPoint;
        QPen pen;
    };
}

#endif // RWAMAPITEM_H
