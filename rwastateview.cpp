#include "rwastateview.h"
#include <QScrollArea>

RwaStateView::RwaStateView(QWidget* parent, RwaScene *scene, QString name)
: RwaGraphicsView(parent, scene, name)
{
    setAcceptDrops(true);
    setAlignment(Qt::AlignTop);

    windowSplitter = new QSplitter(this);
    connect(windowSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(handleSplitter(int, int)));

    layout = new QBoxLayout(QBoxLayout::LeftToRight,this);
    layout->setContentsMargins(0,0,0,0);

    assetList = new RwaAssetList(this, scene);
    assetAttributes = new RwaAssetAttributeView(this, scene);
    assetAttributes->scrollArea->setFixedWidth(260);
    assetAttributes->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    currentPoint = nullptr;
    editStateRadius = false;
    editStateHeight = false;
    editStateWidth = false;
    editStatePosition = false;
    stateRadiusVisible = true;
    assetReflectionsVisible = true;
    assetsVisible = true;
    onlyAssetsOfCurrentStateVisible = true;
    onlyRadiiOfCurrentStateVisible = true;

    connect(backend, SIGNAL(sendMovePixmapsOfCurrentState1(double, double)),
              this, SLOT(movePixmapsOfCurrentState(double,double)));

    connect(backend, SIGNAL(sendMoveCurrentScene()),
              this, SLOT(moveCurrentState()));

    connect(this, SIGNAL(sendMoveCurrentState1(double, double)),
              backend, SLOT(receiveMoveCurrentState1(double,double)));

    connect(backend, SIGNAL(sendMovePixmapsOfCurrentAsset1(double, double)),
              this, SLOT(movePixmapsOfCurrentAsset(double,double)));

    connect(this, SIGNAL(sendMovePixmapsOfCurrentAsset1(double, double)),
              backend, SLOT(receiveMoveCurrentAsset1(double,double)));

    connect(this, SIGNAL(sendMoveCurrentAssetChannel(double, double, int)),
              backend, SLOT(receiveMoveCurrentAssetChannel(double,double, int)));

    connect(this, SIGNAL(sendMoveCurrentAssetReflection(double, double, int)),
              backend, SLOT(receiveMoveCurrentAssetReflection(double,double, int)));

    connect(backend, SIGNAL(sendStatePosition(QPointF)),
              this, SLOT(setMapCoordinates(QPointF)));

    connect(mc, SIGNAL(mouseEventCoordinate(const QMouseEvent*, const QPointF)),
              this, SLOT(receiveMouseMoveEvent(const QMouseEvent*, const QPointF)));

    connect(mc, SIGNAL(sendMouseDownEvent(const QMouseEvent*, const QPointF)),
              this, SLOT(receiveMouseDownEvent(const QMouseEvent*, const QPointF)));

    connect(mc, SIGNAL(sendMouseReleaseEvent()),
              this, SLOT(receiveMouseReleaseEvent()));

    connect(assetList, SIGNAL(newAsset(const QString &, qint32)),
              this, SLOT(addAssetItem(const QString &, qint32)));

    connect(assetList, SIGNAL(deleteAsset(const QString &)),
              this, SLOT(deleteAssetItem(const QString &)));

    connect(this, SIGNAL(sendCurrentAsset(RwaAsset1*)),
            backend, SLOT(receiveLastTouchedAsset(RwaAsset1*)));

    connect(this, SIGNAL(sendCurrentState(RwaState*)),
            backend, SLOT(receiveLastTouchedState(RwaState*)));

    connect(this, SIGNAL(sendCurrentStateRadiusEdited()),
            backend, SLOT(receiveCurrentStateRadiusEdited()));

    connect(backend, SIGNAL(sendCurrentStateRadiusEdited()),
              this, SLOT(receiveUpdateCurrentStateRadius()));

    connect(backend, SIGNAL(sendCurrentStateWithoutRepositioning(RwaState *)),
              this, SLOT(setCurrentStateWithoutRepositioning(RwaState *)));

    addZoomButtons();

    windowSplitter->addWidget(assetAttributes->scrollArea);
    windowSplitter->addWidget(assetList);
    windowSplitter->addWidget(mc);
    layout->addWidget(windowSplitter);

    readSplitterLayout();

    QTimer::singleShot(1000, this, SLOT(correctMapView()));
}

void RwaStateView::keyPressEvent(QKeyEvent *event)
{
    QGraphicsView::keyPressEvent(event);
    switch (event->key())
    {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // qDebug() << "Enter";
            break;
        case Qt::Key_Escape:
            // qDebug() << "Escape";
            break;
        case Qt::Key_Insert:
            // qDebug() << "Insert";
            break;

        default:
            // qDebug() << "RwaStateView::keyPressEvent" << event->key();
            break;
     }
}

void RwaStateView::correctMapView()
{
    mc->resize(QSize(windowSplitter->sizes().at(2), this->height()));
}

void RwaStateView::receiveUpdateCurrentStateRadius()
{
    redrawRadiusOfCurrentState();
}

void RwaStateView::zoomIn()
{
    mc->zoomIn();
    if(currentState)
    {
        currentState->setZoom( mc->currentZoom());
        if(stateRadiusVisible)
            redrawStateRadii();
    }
}

void RwaStateView::zoomOut()
{
    mc->zoomOut();
    if(currentState)
    {
        currentState->setZoom( mc->currentZoom());
        if(stateRadiusVisible)
            redrawStateRadii();
    }
}

void RwaStateView::setZoomLevel(qint32 zoomLevel)
{
    if(!currentState)
        return;

    currentState->setZoom(zoomLevel);
    mc->setZoom(zoomLevel);
    if(stateRadiusVisible)
        redrawStateRadii();
}

void RwaStateView::setCurrentAsset(RwaAsset1 *asset)
{
    if(!asset)
        return;

    RwaGraphicsView::setCurrentAsset(asset);
}

void RwaStateView::redrawStateRadii()
{
    stateRadiusLayer->clearGeometries();
    RwaState *state;
    foreach (state, currentScene->getStates())
    {
        if(state->isGpsState && (state == currentState) )
             drawArea(state, stateRadiusLayer);
    }
}

void RwaStateView::setMapCoordinates(double lon, double lat)
{
    mc->setView(QPointF(lon,lat));
}

void RwaStateView::setMapCoordinates(QPointF coordinates)
{
    mc->setView(coordinates);
}

void RwaStateView::adaptSize(qint32 width, qint32 height)
{
    (void) width;
    (void) height;
    mc->resize(QSize(windowSplitter->sizes().at(2), this->height()));
}

void RwaStateView::handleSplitter(int pos, int index)
{
    (void) pos;
    (void) index;
    mc->resize(QSize(windowSplitter->sizes().at(2), this->height()));
}

void RwaStateView::receiveMouseMoveEvent(const QMouseEvent*, const QPointF myPoint)
{
    if(!currentState)
        return;

    QPointF lastCoordinate;
    double dx, dy;
    std::vector<double> tmp(2, 0.0);

    RwaUtilities::copyLocationCoordinates2Clipboard(myPoint);
    if(backend->logCoordinates)
        RwaUtilities::logLocationCoordinates(myPoint);

    // Assets, their channels, start points and reflections are dragged while the
    // simulation runs as well: the runtime reads their positions every tick, so the
    // sound follows the drag (RwaAsset1::setCoordinates keeps the live position in
    // step). A state change of the running simulation may select another asset
    // mid-drag; the drag then ends rather than moving the newly selected one.
    if(dragType >= 0 && dragAsset == currentAsset)
    {
        if(currentAsset->getLockPosition())
            return;

        tmp = {myPoint.x(), myPoint.y()};

        if(dragType == RWAPOSITIONTYPE_ASSET)
        {
            lastCoordinate = QPointF(currentAsset->getCoordinates()[0], currentAsset->getCoordinates()[1]);
            dx = myPoint.x() - lastCoordinate.x();
            dy = myPoint.y() - lastCoordinate.y();
            currentAsset->setCoordinates(tmp);
            currentAsset->moveMyChildren(dx, dy);
            emit sendMovePixmapsOfCurrentAsset1(dx, dy);
            setUndoAction("Move Asset");
        }

        if(dragType == RWAPOSITIONTYPE_ASSETSTARTPOINT)
        {
            currentAsset->setStartPosition(tmp);
            emit backend->sendAssetPositionsChanged();
            setUndoAction("Move Asset start location.");
        }

        if(dragType == RWAPOSITIONTYPE_ASSETCHANNEL &&
             currentAsset->customChannelPositionsEnabled() )
        {
            lastCoordinate = QPointF(currentAsset->channelcoordinates[dragChannel][0], currentAsset->channelcoordinates[dragChannel][1]);
            dx = myPoint.x() - lastCoordinate.x();
            dy = myPoint.y() - lastCoordinate.y();
            currentAsset->setChannelCoordinate(dragChannel, tmp);

            currentAsset->hasCustomChannelPosition[dragChannel] = true;
            if(RwaMapItem *geo = findAssetItem(currentAsset, dragType, dragChannel))
                geo->setCoordinate(myPoint);
            emit sendMoveCurrentAssetChannel(dx, dy, dragChannel);
            setUndoAction("Move Asset channel position.");
        }

        if(dragType == RWAPOSITIONTYPE_REFLECTIONPOSITION)
        {
            lastCoordinate = QPointF(currentAsset->reflectioncoordinates[dragChannel][0], currentAsset->reflectioncoordinates[dragChannel][1]);
            dx = myPoint.x() - lastCoordinate.x();
            dy = myPoint.y() - lastCoordinate.y();
            currentAsset->setReflectionCoordinate(dragChannel, tmp);
            if(RwaMapItem *geo = findAssetItem(currentAsset, dragType, dragChannel))
                geo->setCoordinate(myPoint);
            emit sendMoveCurrentAssetReflection(dx, dy, dragChannel);
            setUndoAction("Move Asset reflection position.");
        }

        return;
    }

    // The state's area is what the running simulation evaluates:
    // like in the Map View it is edited only while the simulation is stopped.
    if(editArea && !backend->isSimulationRunning())
    {
        resizeArea(myPoint, currentState);
        emit sendCurrentStateRadiusEdited();
        setUndoAction("Edit State area.");
        if(stateRadiusVisible)
            stateRadiusLayer->setVisible(true);
    }
}

void RwaStateView::receiveMouseDownEvent(const QMouseEvent *event, const QPointF myPoint)
{
    if(!currentState)
        return;

    currentPoint = nullptr;
    RwaAsset1 *asset;

    RwaUtilities::copyLocationCoordinates2Clipboard(myPoint);
    if(backend->logCoordinates)
        RwaUtilities::logLocationCoordinates(myPoint);

    if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonDblClick)
    {
        if(currentState->getAreaType() == RWAAREATYPE_POLYGON)
        {
            if(mouseDoubleClickArea(myPoint, currentState))
            {
                redrawStateRadii();
                setUndoAction("Edit State area.");
                return;
            }
        }
    }

    if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonPress)
    {
        if(mouseDownArea(myPoint, currentState))
            return;

        QmapPoint tmppoint(myPoint.x(), myPoint.y());

        for (int i=0; i<assetLayer->geometries.count(); i++)
        {
            if (assetLayer->geometries.at(i)->isVisible() && assetLayer->geometries.at(i)->Touches(&tmppoint, mapadapter))
            {
                 RwaMapItem *item = static_cast<RwaMapItem *>(assetLayer->geometries.at(i));
                 asset = static_cast<RwaAsset1 *>(item->data);

                 if(item->getRwaType() != RWAPOSITIONTYPE_ASSETCHANNEL || asset->customChannelPositionsEnabled())
                 {
                     dragAsset = asset;
                     dragType = item->getRwaType();
                     dragChannel = item->getChannel();
                     emit sendCurrentAsset(asset);
                     mc->setMouseMode(MapControl::None);
                     return;
                 }
             }
         }

        for (int i=0; i<assetReflectionLayer->geometries.count(); i++)
        {
            if (assetReflectionLayer->geometries.at(i)->isVisible() && assetReflectionLayer->geometries.at(i)->Touches(&tmppoint, mapadapter))
            {
                 RwaMapItem *item = static_cast<RwaMapItem *>(assetReflectionLayer->geometries.at(i));
                 asset = static_cast<RwaAsset1 *>(item->data);
                 asset->currentReflection = item->getChannel();
                 dragAsset = asset;
                 dragType = item->getRwaType();
                 dragChannel = item->getChannel();
                 emit sendCurrentAsset(asset);
                 mc->setMouseMode(MapControl::None);
                 return;
             }
         }
     }
}

void RwaStateView::receiveMouseReleaseEvent()
{
    if(currentState)
    {
        if(editArea)
            currentState->setAreaType(currentState->getAreaType());
    }

    editAreaRadius = false;
    editAreaHeight = false;
    editAreaWidth = false;
    editStatePosition = false;
    editAreaCorner = false;
    editArea = false;
    mc->setMouseMode(MapControl::Panning);
    currentPoint = nullptr;
    dragAsset = nullptr;
    dragType = -1;
    dragChannel = -1;
    writeUndo();
}

void RwaStateView::dropEvent(QDropEvent *event)
{
    (void) event;
    if(backend->logOther)
        qDebug();
}

void RwaStateView::moveCurrentState()
{
    mc->setView(QPointF(currentState->getCoordinates()[0], currentState->getCoordinates()[1]));
    if(currentState->childrenDoFollowMe())
        redrawAssetsOfCurrentState();

    if(stateRadiusVisible)
        redrawStateRadii();
}

void RwaStateView::setCurrentState(RwaState *state)
{
    if(!state)
        return;

    QDockWidget *window = static_cast<QDockWidget *>(parent());
    window->setWindowTitle("State View - "+ QString::fromStdString(state->objectName()));
    mc->setView(QPointF(state->getCoordinates()[0], state->getCoordinates()[1]));
    setMap2AreaZoomLevel(state);
    RwaGraphicsView::setCurrentState(state);

    if(QObject::sender() != this->backend)
    {
        qDebug();
        emit sendCurrentAsset(state->lastTouchedAsset);
    }
}

void RwaStateView::setCurrentScene(RwaScene *scene)
{
    if(!scene)
        return;

    if(scene == currentScene)
        return;

    RwaGraphicsView::setCurrentScene(scene);

    if(QObject::sender() != this->backend)
    {
        qDebug();
        emit sendCurrentState(scene->lastTouchedState);
    }
}

void RwaStateView::setCurrentStateWithoutRepositioning(RwaState *state)
{
    if(!state)
        return;

    QDockWidget *window = static_cast<QDockWidget *>(parent());
    window->setWindowTitle("State View - "+ QString::fromStdString(state->objectName()));
    RwaGraphicsView::setCurrentState(state);

    if(QObject::sender() != this->backend)
        emit sendCurrentAsset(state->lastTouchedAsset);
}

int RwaStateView::getNumberOfSelectedAssets()
{
    return assetList->getNumberOfSelectedAssets();
}

void RwaStateView::addAssetItem(const QString &path, qint32 type)
{
    if(currentState)
    {
        string uid = std::string(QUuid::createUuid().toString().toLatin1());
        RwaAsset1 *newItem = new RwaAsset1(path.toStdString(), currentState->getCoordinates(), type, uid);
        newItem->refreshFileProperties();
        currentState->addAsset(newItem);
        currentState->setLastTouchedAsset(newItem);
        emit sendCurrentState(currentState);
    }
}

void RwaStateView::deleteAssetItem(const QString &path)
{
    if(backend->isSimulationRunning()) // the runtime's activeAssets would keep a pointer to the deleted asset
    {
        qWarning() << "Cannot delete an asset while the simulation is running.";
        return;
    }

    if(currentState)
    {
        RwaAsset1 *item = currentState->getAsset(path.toStdString());
        if(!item)
            return;

        QFile file(QString::fromStdString(item->getFullPath()));

        if(backend->trashAsset && !backend->fileUsedByAnotherAsset(item))
        {
            // the undo history can resurrect the asset entry, so the file goes
            // to the session trash, where an undo restore finds it again; only
            // if that fails, to the system trash (a manual recovery path)
            if(file.exists() && !backend->moveAsset2SessionTrash(QString::fromStdString(item->getFullPath())))
            {
                if(!file.moveToTrash())
                    file.remove();
            }
        }

        currentState->deleteAsset(path.toStdString());
        emit sendCurrentState(currentState);
        emit sendWriteUndo("Delete Asset");
    }
}
