#ifndef RWAASSETVIEW_H
#define RWAASSETVIEW_H

#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QComboBox>
#include <QListWidget>
#include <QMediaPlayer>
#include <qmediaplayer.h>
#include "rwaassetlist.h"
#include "rwagraphicsview.h"
#include "rwaassetattributeview.h"

class RwaStateView : public RwaGraphicsView
{
    Q_OBJECT
public:
    explicit RwaStateView(QWidget* parent = nullptr, RwaScene *scene = nullptr, QString name = "");
private:

    QmapPoint *currentPoint;
    CirclePoint *currentRadius;
    RectPoint *currentRect;
    RwaAssetList *assetList;
    RwaAssetAttributeView *assetAttributes;

    // What the mouse grabbed at mouse-down: the asset, the marker's RWAPOSITIONTYPE_*
    // (-1 = no drag) and its channel / reflection number. Not the marker itself: the
    // layers are rebuilt on every state change, also by the running simulation, which
    // deletes the markers - the drag looks its marker up again on every move.

    /** Asset grabbed for dragging */
    RwaAsset1 *dragAsset = nullptr;
    /** RWAPOSITIONTYPE_* (-1 = no drag) */
    int dragType = -1;
    /** Asset channel / reflection number */
    int dragChannel = -1;

    void zoomIn() override;
    void zoomOut() override;

public slots:

    void setCurrentState(RwaState *currentState) override;
    void setCurrentAsset(RwaAsset1 *asset) override;
    void setCurrentScene(RwaScene *scene) override;
    void setCurrentStateWithoutRepositioning(RwaState *state);

    void receiveMouseMoveEvent(const QMouseEvent*, const QPointF) override;
    void receiveMouseReleaseEvent() override;
    void receiveMouseDownEvent(const QMouseEvent*, const QPointF) override;
    void dropEvent(QDropEvent *event) override;

    void handleSplitter(int pos, int index);
    void setMapCoordinates(double lon, double lat);
    void setMapCoordinates(QPointF coordinates);
    void setZoomLevel(qint32 zoomLevel);
    void adaptSize(qint32 width, qint32 height) override;
    void moveCurrentState();

    void addAssetItem(const QString &path, qint32 type);
    void deleteAssetItem(const QString &path);
    int getNumberOfSelectedAssets();
    void redrawStateRadii();
    void receiveUpdateCurrentStateRadius();

protected:
    void keyPressEvent(QKeyEvent *event) override;

signals:

    void sendNewAsset(RwaState *currentState, RwaAsset1 *item);

private slots:
    void correctMapView();
private:
    bool editStateRadius;
    bool editStateWidth;
    bool editStateHeight;
    bool editStatePosition;
};

#endif // RWAASSETVIEW_H
