#include "rwamapitem.h"

namespace qmapcontrol
{
    RwaMapItem::RwaMapItem(QPointF position, RwaLocation1 *rwaItem, int rwaType, QPixmap *pixmap, int channel) :
        QmapPoint(position.x(), position.y(), pixmap, rwaItem)
    {
        this->rwaItem = rwaItem;
        this->rwaType = rwaType;
        this->channel = channel;
    }

    RwaLocation1 *RwaMapItem::getRwaItem() const
    {
        return rwaItem;
    }

    int RwaMapItem::getRwaType() const
    {
        return rwaType;
    }

    int RwaMapItem::getChannel() const
    {
        return channel;
    }

    RwaTrajectory::RwaTrajectory(RwaLocation1 *rwaItem, const QPen &pen) :
        QmapPoint(0, 0, QString::fromStdString(rwaItem->objectName())),
        rwaItem(rwaItem), pen(pen)
    {
        setData(rwaItem);
        setAllowTouches(false);
    }

    RwaLocation1 *RwaTrajectory::getRwaItem() const
    {
        return rwaItem;
    }

    void RwaTrajectory::setEndpoints(const QPointF &from, const QPointF &to)
    {
        if(fromPoint == from && toPoint == to)
            return;
        fromPoint = from;
        toPoint = to;
        setCoordinate(from); // X/Y, and the repaint request that goes with a moved point
    }

    void RwaTrajectory::setTrajectoryPen(const QPen &value)
    {
        pen = value;
    }

    void RwaTrajectory::draw(QPainter *painter, const MapAdapter *mapadapter, const QRect &, const QPoint)
    {
        if(!visible)
            return;
        // like LineString::draw: the painter is already translated to map pixels,
        // coordinateToDisplay() is all that is needed
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(pen);
        painter->drawLine(mapadapter->coordinateToDisplay(fromPoint), mapadapter->coordinateToDisplay(toPoint));
        painter->restore();
    }

    bool RwaTrajectory::Touches(QmapPoint *, const MapAdapter *)
    {
        return false;
    }
}
