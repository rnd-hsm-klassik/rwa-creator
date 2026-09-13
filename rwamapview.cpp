#include "rwamapview.h"
#include "rwalandmarkdialog.h"
#include "rwaheadtrackerconnect.h"
#include <QMenu>
#include <QDateTime>

RwaMapView::RwaMapView(QWidget* parent, RwaScene *scene, QString name)
: RwaGraphicsView(parent, scene, name)
{
    mc->setParent(this);
    qint32 toolbarFlags = (  RWATOOLBAR_MAPEDITTOOLS
                           | RWATOOLBAR_SCENEMENU
                           | RWATOOLBAR_SIMULATORTOOLS
                           | RWATOOLBAR_SCENEMENU
                           | RWATOOLBAR_SELECTSCENEMENU
                           | RWATOOLBAR_STATEMENU
                           | RWATOOLBAR_SELECTSTATEMENU
                           | RWATOOLBAR_MAINVOLUME
                           | RWATOOLBAR_SUGGESTPLACES);

    stateLineEditVisible = true;
    assetsVisible = false;
    stateRadiusVisible = false;
    assetReflectionsVisible = false;
    sceneRadiusVisible = true;
    statesVisible = true;
    scenesVisible = true;

    tool = RWATOOL_ARROW;

    layout = new QBoxLayout(QBoxLayout::TopToBottom,this);
    layout->setSpacing(1);
    layout->setContentsMargins(QMargins(1,1,1,1));

    currentScene = nullptr;
    mc->setZoom(13);

    // MapControl Notifications

    connect(mc, SIGNAL(mouseEventCoordinate(const QMouseEvent*, const QPointF)),
              this, SLOT(receiveMouseMoveEvent(const QMouseEvent*, const QPointF)));

    connect(mc, SIGNAL(sendMouseDownEvent(const QMouseEvent*, const QPointF)),
              this, SLOT(receiveMouseDownEvent(const QMouseEvent*, const QPointF)));

    connect(mc, SIGNAL(sendSelectRect(QRectF)),
              this, SLOT(receiveSelectRect(QRectF)));

    connect(mc, SIGNAL(sendMouseReleaseEvent()),
              this, SLOT(receiveMouseReleaseEvent()));

    // Backend Notifications

    connect(this, SIGNAL(sendEntityPosition(QPointF)),
              backend, SLOT(receiveEntityPosition(QPointF)));

    connect(this, SIGNAL(sendMapPosition(QPointF)),
              backend, SLOT(receiveMapCoordinates(QPointF)));

    connect(this, SIGNAL(sendStateCoordinate(QPointF)),
              backend, SLOT(receiveStatePosition(QPointF)));

//    connect(this, SIGNAL(sendStartStopSimulator(bool)),
//              backend, SLOT(startStopSimulator(bool)));

    connect(this, SIGNAL(sendCurrentState(RwaState*)),
              backend, SLOT(receiveLastTouchedState(RwaState*)));

    connect(this, SIGNAL(sendCurrentScene(RwaScene*)),
              backend, SLOT(receiveLastTouchedScene(RwaScene*)));

    connect(this, SIGNAL(sendCurrentSceneWithoutRepositioning(RwaScene *)),
              backend, SLOT(receiveCurrentSceneWithouRepositioning(RwaScene *)));

    connect(this, SIGNAL(sendMoveCurrentState1(double, double)),
              backend, SLOT(receiveMoveCurrentState1(double, double)));

    connect(this, SIGNAL(sendMoveCurrentScene()),
              backend, SLOT(receiveMoveCurrentScene()));

    connect(this, SIGNAL(sendSelectedStates(QStringList)),
              backend, SLOT(receiveSelectedStates(QStringList)));

    connect(backend, SIGNAL(sendMovePixmapsOfCurrentAsset1(double, double)),
              this, SLOT(movePixmapsOfCurrentAsset(double,double)));

    connect(backend, SIGNAL(updateScene(RwaScene *)),
              this, SLOT(setCurrentScene(RwaScene *)));

    connect(backend, SIGNAL(sendMovePixmapsOfCurrentState1(double, double)),
              this, SLOT(movePixmapsOfCurrentState(double,double)));

    connect(backend, SIGNAL(sendMoveCurrentAssetChannel(double, double, int)),
              this, SLOT(movePixmapsOfCurrentAssetChannel(double,double, int)));

    connect(backend, SIGNAL(sendMoveCurrentAssetReflection(double, double, int)),
              this, SLOT(movePixmapsOfCurrentAssetReflection(double,double, int)));

    connect(this, SIGNAL(sendCurrentStateRadiusEdited()),
            backend, SLOT(receiveCurrentStateRadiusEdited()));

    connect(this, SIGNAL(sendCurrentSceneRadiusEdited()),
            backend, SLOT(receiveCurrentSceneRadiusEdited()));

    connect(backend, SIGNAL(sendCurrentStateRadiusEdited()),
              this, SLOT(receiveUpdateCurrentStateRadius()));

    connect(backend, SIGNAL(sendCurrentSceneRadiusEdited()),
              this, SLOT(receiveUpdateCurrentSceneRadius()));

    // Landmarks: redrawn whenever the backend's list changes (record, delete,
    // load, undo). newGameLoaded is also what clears the layers (initNewGame).
    connect(backend, SIGNAL(sendLandmarksChanged()), this, SLOT(redrawLandmarks()));
    connect(backend, SIGNAL(newGameLoaded()), this, SLOT(redrawLandmarks()));
    connect(backend, SIGNAL(undoGameLoaded()), this, SLOT(redrawLandmarks()));

    // Key events (Delete on a selected landmark) come through the map widget.
    mc->setFocusPolicy(Qt::ClickFocus);

    connect(backend, SIGNAL(sendMoveHero2CurrentState()),
              this, SLOT(setEntityCoordinates2CurrentState()));

    connect(backend, SIGNAL(sendMoveHero2CurrentScene()),
              this, SLOT(setEntityCoordinates2CurrentScene()));

    connect(backend, SIGNAL(sendHeroPositionEdited()),
              this, SLOT(receiveHeroPositionEdited()));

    connect(backend, SIGNAL(sendCurrentSceneWithoutRepositioning(RwaScene *)),
              this, SLOT(setCurrentSceneWithoutRepositioning(RwaScene *)));

    layout->addWidget(setupToolbar(toolbarFlags));
    layout->addWidget(mc);
    addZoomButtons();

    // Toolbox Notifications

    connect(toolbar,    SIGNAL(sendMapCoordinates(double,double)),
        this, SLOT(setMapCoordinates(double,double)));

    connect(this,    SIGNAL(sendMapCoordinates(double,double)),
        toolbar, SLOT(receiveMapCoordinates(double,double)));

    readSettings();
}

RwaMapView::~RwaMapView()
{
    writeSettings();
}

void RwaMapView::readSettings()
{
    QSettings settings;
    setAssetsVisible(settings.value("mapviewassetsvisible").toBool());
    toolbar->assetsVisibleButton->setChecked(settings.value("mapviewassetsvisible").toBool());
    setRadiiVisible(settings.value("mapviewradiivisible").toBool());
    toolbar->radiiVisibleButton->setChecked(settings.value("mapviewradiivisible").toBool());
    setLandmarksVisible(settings.value("mapviewlandmarksvisible", true).toBool());
    toolbar->landmarksVisibleButton->setChecked(landmarksVisible);

    // This is not so nice but much less code then putting it in backend and add more slots and signals for very basic functionality
    backend->heroFollowsSceneAndState = settings.value("mapviewherofollows").toBool();
    toolbar->heroFollowsSceneAndStateButton->setChecked(settings.value("mapviewherofollows").toBool());
}

void RwaMapView::writeSettings()
{
    QSettings settings;
    settings.setValue("mapviewassetsvisible", assetsVisible);
    settings.setValue("mapviewradiivisible", stateRadiusVisible);
    settings.setValue("mapviewlandmarksvisible", landmarksVisible);
    settings.setValue("mapviewherofollows", backend->heroFollowsSceneAndState);
}

void RwaMapView::setMapCoordinates(double lon, double lat)
{
    qDebug();
    mc->setView(QPointF(lon,lat));
    if(currentScene)
    {
        redrawStates();
        emit sendMapPosition(mc->currentCoordinate());
        emit sendMapCoordinates(mc->currentCoordinate().x(), mc->currentCoordinate().y());
    }
}

void RwaMapView::zoomIn()
{
    mc->zoomIn();
    if(currentScene)
        currentScene->setZoom( mc->currentZoom());
    if(stateRadiusVisible)
         redrawStateRadii();

    redrawStates();
    redrawSceneRadii();
}

void RwaMapView::zoomOut()
{
    mc->zoomOut();
    if(currentScene)
        currentScene->setZoom( mc->currentZoom());
    if(stateRadiusVisible)
         redrawStateRadii();

    redrawStates();
    redrawSceneRadii();
}

void RwaMapView::adaptSize(qint32 width, qint32 height)
{
    //resize(QSize(width, height));
    mc->resize(QSize(width-20, height-70));
}

void RwaMapView::moveCurrentScene(const QPointF myPoint)
{
    double dx, dy;
    std::vector<double> lastCoordinate(2, 0.0);
    std::vector<double> tmp(2, 0.0);
    QmapPoint *geo = static_cast<QmapPoint *>(currentScenePoint);

    if(geo)
    {
        if(currentScene->positionIsLocked())
            return;

        geo->setCoordinate(myPoint);

        lastCoordinate = currentScene->getCoordinates();
        tmp[0] = myPoint.x();
        tmp[1] = myPoint.y();
        currentScene->setCoordinates(tmp);

        dx = currentScene->getCoordinates()[0] - lastCoordinate[0];
        dy = currentScene->getCoordinates()[1] - lastCoordinate[1];
        geo->move(dx, dy);

        if(currentScene->childrenDoFollowMe())
        {
            currentScene->moveMyChildren(dx, dy);
            movePixmapsOfCurrentScene(dx,dy);
            emit sendMoveCurrentScene();
        }

        currentScene->moveCorners(dx, dy);
        redrawSceneRadii();
        return;
    }
}

void RwaMapView::moveCurrentState1(const QPointF myPoint)
{
    double dx, dy;
    std::vector<double> lastCoordinate(2, 0.0);
    std::vector<double> tmp(2, 0.0);
    QmapPoint *geo = static_cast<QmapPoint *>(currentStatePoint);

    if(geo)
    {
        if(currentState->positionIsLocked())
            return;

        lastCoordinate = currentState->getCoordinates();
        tmp[0] = myPoint.x();
        tmp[1] = myPoint.y();
        currentState->setCoordinates(tmp);

        dx = currentState->getCoordinates()[0] - lastCoordinate[0];
        dy = currentState->getCoordinates()[1] - lastCoordinate[1];
        if(currentState->childrenDoFollowMe())
            currentState->moveMyChildren(dx, dy);

        emit sendMoveCurrentState1(dx, dy);
        emit sendStateCoordinate(myPoint);

        return;
    }
}

void RwaMapView::receiveMouseMoveEvent(const QMouseEvent*, const QPointF myPoint)
{
    if(tool != RWATOOL_ARROW)
        return;

    QmapPoint *geo;
    geo = static_cast<QmapPoint *>(currentLandmarkPoint);
    if(geo)
    {
        geo->setCoordinate(myPoint);
        RwaLandmark *landmark = static_cast<RwaLandmark *>(geo->data);
        landmark->setCoordinates({myPoint.x(), myPoint.y()});
        setUndoAction("Move Landmark");
        return;
    }

    if(!backend->isSimulationRunning())
    {
        geo = static_cast<QmapPoint *>(currentStatePoint);
        if(geo)
        {
            moveCurrentState1(myPoint);
            if(!currentState->positionIsLocked())
                setUndoAction("Move State");
            return;
        }

        geo = static_cast<QmapPoint *>(currentScenePoint);
        if(geo)
        {
            moveCurrentScene(myPoint);
            if(!currentScene->positionIsLocked())
                setUndoAction("Move Scene");
            return;
        }

        if(editArea)
        {
            if(editSceneArea)
            {
                resizeArea(myPoint, currentScene);
                setUndoAction("Resize Scene Area");
                emit sendCurrentSceneRadiusEdited();
                sceneRadiusLayer->setVisible(true);
            }
            if(editStateArea)
            {
                resizeArea(myPoint, currentState);
                setUndoAction("Resize State Area");
                emit sendCurrentStateRadiusEdited();
                stateRadiusLayer->setVisible(true);
            }
            return;
        }
    }

    geo = static_cast<QmapPoint *>(currentEntityPoint);
    if(geo)
    {
        geo->setCoordinate(myPoint);
        currentEntity = (RwaEntity *)geo->data;
        std::vector<double> tmp {myPoint.x(), myPoint.y()};
        currentEntity->setCoordinates(tmp);
        emit sendEntityPosition(myPoint);
        return;
    }
    if(tool == RWATOOL_PEN)
    {
        mc->setSelectSize(myPoint.x()-selectRectX, myPoint.y()-selectRectY);
        mc->updateRequestNew();
        return;
    }

    emit sendMapPosition(mc->currentCoordinate());
    emit sendMapCoordinates(mc->currentCoordinate().x(), mc->currentCoordinate().y());
    currentScene->currentViewCoordinates[0] = mc->currentCoordinate().x();
    currentScene->currentViewCoordinates[1] = mc->currentCoordinate().y();
    RwaUtilities::copyLocationCoordinates2Clipboard(mc->currentCoordinate());

    if(backend->logCoordinates)
        RwaUtilities::logLocationCoordinates(mc->currentCoordinate());
}

bool RwaMapView::mouseDownEntities(const QPointF myPoint)
{
    currentEntityPoint = nullptr;
    QmapPoint* tmppoint = new QmapPoint(myPoint.x(), myPoint.y());
    for (int i=0; i<entityLayer->geometries.count(); i++)
    {
        if (entityLayer->geometries.at(i)->isVisible() && entityLayer->geometries.at(i)->Touches(tmppoint, mapadapter))
        {
             currentEntityPoint = (QmapPoint *)entityLayer->geometries.at(i);
             currentEntity= (RwaEntity *)currentEntityPoint->data;
             mc->setMouseMode(MapControl::None);
             break;
        }
    }

    if(currentEntityPoint)
    {
        delete tmppoint;
        return true;
    }
    else
        return false;
}

bool RwaMapView::mouseDownScenes(const QPointF myPoint)
{
    currentScenePoint = nullptr;
    QmapPoint* tmppoint = new QmapPoint(myPoint.x(), myPoint.y());
    for (int i=0; i<scenesLayer->geometries.count(); i++)
    {
        if (scenesLayer->geometries.at(i)->isVisible() && scenesLayer->geometries.at(i)->Touches(tmppoint, mapadapter))
        {
             currentScenePoint = (QmapPoint *)scenesLayer->geometries.at(i);
             currentScene = (RwaScene *)currentScenePoint->data;
             mc->setMouseMode(MapControl::None);
             scenesLayer->setVisible(true);
        }
    }

    if(currentScenePoint)
    {
        delete tmppoint;
        return true;
    }
    else
        return false;
}

bool RwaMapView::mouseDownStates(const QPointF myPoint)
{
    currentStatePoint = nullptr;
    QmapPoint* tmppoint = new QmapPoint(myPoint.x(), myPoint.y());
    for (int i=0; i<statesLayer->geometries.count(); i++)
    {
        if (statesLayer->geometries.at(i)->isVisible() && statesLayer->geometries.at(i)->Touches(tmppoint, mapadapter))
        {
             currentStatePoint = (QmapPoint *)statesLayer->geometries.at(i);
             currentState = (RwaState *)currentStatePoint->data;
             mc->setMouseMode(MapControl::None);

             tmpObjectName = QString::fromStdString( currentState->objectName());
             emit sendCurrentState(currentState);
             statesLayer->setVisible(true);
        }
    }

    if(currentStatePoint)
    {
        delete tmppoint;
        return true;
    }
    else
        return false;
}

/** ************************************* Landmarks ************************************* */

RwaLandmark *RwaMapView::landmarkAt(const QPointF myPoint, QmapPoint **point)
{
    QmapPoint tmppoint(myPoint.x(), myPoint.y());
    for (int i=0; i<landmarkLayer->geometries.count(); i++)
    {
        if (landmarkLayer->geometries.at(i)->isVisible() && landmarkLayer->geometries.at(i)->Touches(&tmppoint, mapadapter))
        {
            QmapPoint *hit = static_cast<QmapPoint *>(landmarkLayer->geometries.at(i));
            if(point)
                *point = hit;
            return static_cast<RwaLandmark *>(hit->data);
        }
    }
    return nullptr;
}

bool RwaMapView::mouseDownLandmarks(const QPointF myPoint)
{
    currentLandmarkPoint = nullptr;
    QmapPoint *point = nullptr;
    RwaLandmark *landmark = landmarkAt(myPoint, &point);
    if(!landmark)
        return false;

    currentLandmarkPoint = point;
    selectLandmark(landmark);
    mc->setMouseMode(MapControl::None);
    return true;
}

void RwaMapView::selectLandmark(RwaLandmark *landmark)
{
    currentLandmark = landmark;
    for (int i=0; i<landmarkLayer->geometries.count(); i++)
    {
        QmapPoint *point = static_cast<QmapPoint *>(landmarkLayer->geometries.at(i));
        point->setPixmap(point->data == landmark ? landmarkLayer->getActivePixmap()
                                                 : landmarkLayer->getPassivePixmap());
    }
    mc->updateRequestNew();
}

void RwaMapView::keyPressEvent(QKeyEvent *event)
{
    if((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && currentLandmark)
    {
        deleteLandmark(currentLandmark);
        event->accept();
        return;
    }
    RwaGraphicsView::keyPressEvent(event);
}

/**
 * The flag button: a landmark where the hero stands. With Hero Follows RTK
 * Position that is the live headtracker fix, otherwise wherever the hero was
 * dragged (or moved by OSC imput). The capture data come from the headtracker
 * connection so the dialog can say how trustworthy the position is.
 */
void RwaMapView::recordLandmarkAtHero()
{
    if(backend->simulator->entities.isEmpty() || backend->completeProjectPath.isEmpty())
    {
        qWarning() << "Landmark: no game loaded, nothing to record";
        return;
    }

    RwaEntity *hero = backend->simulator->entities.first();
    RwaLandmark *landmark = new RwaLandmark(backend->nextLandmarkName().toStdString(), hero->getCoordinates());
    landmark->captured = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();

    RwaHeadtrackerConnect *headtracker = RwaHeadtrackerConnect::getInstance();
    if(headtracker->heroFollowsRtkPosition() && headtracker->positionIsLive())
    {
        landmark->source = "rtk";
        if(headtracker->hasFix())
        {
            landmark->carrSoln = headtracker->lastFix().carrSoln;
            landmark->hAccMm = headtracker->lastFix().hAccMm;
        }
    }
    else
        landmark->source = "hand";

    if(RwaLandmarkDialog::edit(this, landmark, true) != RwaLandmarkDialog::Accepted)
    {
        delete landmark;
        return;
    }

    if(!landmarksVisible)
        toolbar->landmarksVisibleButton->click();   // recording something you cannot see helps nobody

    backend->addLandmark(landmark);   // redraws through sendLandmarksChanged
    selectLandmark(landmark);
    qInfo() << "Landmark recorded:" << QString::fromStdString(landmark->objectName())
            << QString::number(landmark->getCoordinates()[1], 'f', 7)
            << QString::number(landmark->getCoordinates()[0], 'f', 7)
            << "(" << RwaLandmarkDialog::captureText(landmark) << ")";
    setUndoAction("New Landmark");
    writeUndo();
}

void RwaMapView::editLandmark(RwaLandmark *landmark)
{
    switch(RwaLandmarkDialog::edit(this, landmark, false))
    {
    case RwaLandmarkDialog::Accepted:
        redrawLandmarks();
        setUndoAction("Edit Landmark");
        writeUndo();
        break;
    case RwaLandmarkDialog::Deleted:
        deleteLandmark(landmark);
        break;
    case RwaLandmarkDialog::Cancelled:
        break;
    }
}

void RwaMapView::deleteLandmark(RwaLandmark *landmark)
{
    if(!landmark)
        return;
    qInfo() << "Landmark deleted:" << QString::fromStdString(landmark->objectName());
    if(currentLandmark == landmark)
        currentLandmark = nullptr;
    currentLandmarkPoint = nullptr;
    backend->removeLandmark(landmark);   // deletes it and redraws
    setUndoAction("Delete Landmark");
    writeUndo();
}

void RwaMapView::moveHeroToLandmark(RwaLandmark *landmark)
{
    if(backend->simulator->entities.isEmpty())
        return;
    RwaEntity *hero = backend->simulator->entities.first();
    hero->setCoordinates(landmark->getCoordinates());
    emit sendEntityPosition(QPointF(landmark->getCoordinates()[0], landmark->getCoordinates()[1]));
    redrawEntities();
}

/**
 * The selected asset's anchor goes to the landmark; channel and reflection
 * positions shift by the same delta, the way a drag in the State View moves
 * them (moveMyChildren). The start position of a moving asset stays, as it does
 * on a drag.
 */
void RwaMapView::moveCurrentAssetToLandmark(RwaLandmark *landmark)
{
    RwaAsset1 *asset = backend->getLastTouchedAssetItem();
    if(!asset)
        return;
    if(asset->getLockPosition())
    {
        qWarning() << "Landmark: asset" << QString::fromStdString(asset->objectName()) << "has a locked position, not moved";
        return;
    }
    const std::vector<double> from = asset->getCoordinates();
    const std::vector<double> to = landmark->getCoordinates();
    const double dx = to[0] - from[0];
    const double dy = to[1] - from[1];
    asset->setCoordinates(to);
    asset->moveMyChildren(dx, dy);
    backend->receiveMoveCurrentAsset1(dx, dy);   // every view shifts its pixmaps of the current asset
    qInfo() << "Asset" << QString::fromStdString(asset->objectName()) << "moved to landmark"
            << QString::fromStdString(landmark->objectName());
    setUndoAction("Move Asset to Landmark");
    writeUndo();
}

void RwaMapView::showLandmarkMenu(RwaLandmark *landmark, const QPoint &globalPos)
{
    QMenu menu(this);

    QAction *moveHero = menu.addAction(tr("Move Hero here"));
    RwaHeadtrackerConnect *headtracker = RwaHeadtrackerConnect::getInstance();
    if(headtracker->heroFollowsRtkPosition())
    {
        moveHero->setEnabled(false);
        moveHero->setText(tr("Move Hero here (hero follows RTK position)"));
    }

    RwaAsset1 *asset = backend->getLastTouchedAssetItem();
    QAction *moveAsset = menu.addAction(asset ? tr("Move \"%1\" here").arg(QString::fromStdString(asset->objectName()))
                                              : tr("Move Asset here (no asset selected)"));
    moveAsset->setEnabled(asset != nullptr);

    menu.addSeparator();
    QAction *edit = menu.addAction(tr("Edit..."));
    QAction *remove = menu.addAction(tr("Delete"));

    QAction *chosen = menu.exec(globalPos);
    if(chosen == moveHero)
        moveHeroToLandmark(landmark);
    else if(chosen == moveAsset)
        moveCurrentAssetToLandmark(landmark);
    else if(chosen == edit)
        editLandmark(landmark);
    else if(chosen == remove)
        deleteLandmark(landmark);
}

void RwaMapView::receiveSelectRect(QRectF selectRect)
{
    RwaState *state;
    foreach (state, currentScene->getStates())
    {
        QRectF tmp;
        tmp.setX(state->getCoordinates()[0]);
        tmp.setY(state->getCoordinates()[1]);
        tmp.setWidth(0.0000001);tmp.setHeight(0.0000001);
        if(selectRect.contains(tmp))
        {
            state->select(true);
            //qDebug("%d", state->getSta);
        }
    }
}

void RwaMapView::mouseDownMarkee(const QMouseEvent *event, const QPointF myPoint)
{
    if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonPress)
    {
        mc->setMouseMode(MapControl::Dragging);
        /*mc->setSelectOrigin(myPoint.x(), myPoint.y());
        selectRectX = myPoint.x();
        selectRectY = myPoint.y();
        qDebug("%f", myPoint.x());*/

    }
}

void RwaMapView::mouseDownArrow(const QMouseEvent *event, const QPointF myPoint)
{
    if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonPress)
    {
        if(mouseDownEntities(myPoint)) // entities have priority
           return;

        if(mouseDownLandmarks(myPoint))
            return;

        // A click anywhere else drops the landmark selection.
        if(currentLandmark)
            selectLandmark(nullptr);

        if(!backend->isSimulationRunning())
        {
            if(mouseDownStates(myPoint))
                return;

            if(mouseDownScenes(myPoint))
                return;

            if(mouseDownArea(myPoint, currentScene))
                return;

            if(mouseDownArea(myPoint, currentState))
                return;
        }
    }

    if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonDblClick)
    {
        if(RwaLandmark *landmark = landmarkAt(myPoint))
        {
            currentLandmarkPoint = nullptr;   // the press before this double click started a drag
            mc->setMouseMode(MapControl::Panning);
            editLandmark(landmark);
            return;
        }
    }

    if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonDblClick && !backend->isSimulationRunning())
    {
        if(mouseDoubleClickArea(myPoint, currentScene))
        {
            setUndoAction("Edit Scene area.");
            sceneRadiusLayer->setVisible(true);
            emit sendCurrentSceneRadiusEdited();
            return;
        }

        if(mouseDoubleClickArea(myPoint, currentState))
        {
            setUndoAction("Edit State area.");
            stateRadiusLayer->setVisible(true);
            emit sendCurrentStateRadiusEdited();
            return;
        }

        std::vector<double> tmp(2, 0.0);
        tmp[0] = myPoint.x();
        tmp[1] = myPoint.y();

        std::string stateName("State "+ std::to_string( RwaBackend::getStateNameCounter(currentScene->getStates())));
        RwaState *newState = currentScene->addState(stateName, tmp);
        emit sendCurrentSceneWithoutRepositioning(currentScene);
        emit sendCurrentState(newState);
        mc->setMouseMode(MapControl::None);
        setUndoAction("New State");
    }
}

void RwaMapView::mouseDownRubber(const QMouseEvent *event, const QPointF myPoint)
{
    QmapPoint* tmppoint = new QmapPoint(myPoint.x(), myPoint.y());
    if (event->button() == Qt::LeftButton && event->type() == QEvent::MouseButtonPress)
    {
        for (int i=0; i<statesLayer->geometries.count(); i++)
        {
            if (statesLayer->geometries.at(i)->isVisible() && statesLayer->geometries.at(i)->Touches(tmppoint, mapadapter))
            {
                 currentStatePoint = (QmapPoint *)statesLayer->geometries.at(i);
                 RwaState *state = (RwaState *)currentStatePoint->data;
                 if(state->isImmortal)
                 {
                     qDebug() << "Fallback/Background States can not be deleted.";
                     delete tmppoint;
                     return;
                 }

                 if(currentState == state)
                     currentState = nullptr;
                 currentStatePoint = nullptr;

                 currentScene->removeState(state);
                 emit sendCurrentScene(currentScene);
                 emit sendCurrentState(currentScene->lastTouchedState);
                 setUndoAction("Remove State");
                 delete tmppoint;
                 return;
            }
        }
    }
    delete tmppoint;
}

void RwaMapView::receiveMouseDownEvent(const QMouseEvent *event, const QPointF myPoint)
{
    RwaUtilities::copyLocationCoordinates2Clipboard(myPoint);
    if(backend->logCoordinates)
        RwaUtilities::logLocationCoordinates(myPoint);

    currentStatePoint = nullptr;

    // right click on a flag: the landmark's context menu.
    // the map widget zooms on every other right click (setRightClickConsumed).
    if(event->button() == Qt::RightButton && event->type() == QEvent::MouseButtonPress)
    {
        if(RwaLandmark *landmark = landmarkAt(myPoint))
        {
            mc->setRightClickConsumed(true);
            selectLandmark(landmark);
            showLandmarkMenu(landmark, event->globalPosition().toPoint());
        }
        return;
    }

    switch(tool)
    {
        case RWATOOL_ARROW:
        {
            mouseDownArrow(event, myPoint);
            break;
        }
        case RWATOOL_RUBBER:
        {
            if(!backend->isSimulationRunning())
                mouseDownRubber(event, myPoint);
            break;
        }
        case RWATOOL_PEN:
        {
            mouseDownMarkee(event, myPoint);
            break;
        }
        default:break;
    }
}

void RwaMapView::receiveMouseReleaseEvent()
{
    if(tool == RWATOOL_PEN)
    {
        QStringList states;
        for (int i=0; i<statesLayer->geometries.count(); i++)
        {
            currentStatePoint = (QmapPoint *)statesLayer->geometries.at(i);
            std::vector<double> position = std::vector<double>(2, 0.0);
            position[0] = currentStatePoint->coordinate().x();
            position[1] = currentStatePoint->coordinate().y();
            std::vector<double> topLeft = std::vector<double>(2, 0.0);
            topLeft[0] = mc->getSelectRect().topLeft().x();
            topLeft[1] = mc->getSelectRect().topLeft().y();
            std::vector<double> bottomRight = std::vector<double>(2, 0.0);
            bottomRight[0] = mc->getSelectRect().bottomRight().x();
            bottomRight[1] = mc->getSelectRect().bottomRight().y();

            if(RwaUtilities::coordinateWithinRectangle1(position, topLeft, bottomRight))
            {
                RwaState *state = (RwaState *)currentStatePoint->data;
                states << QString::fromStdString(state->objectName());
            }
        }

        if(editSceneArea)
        {
            emit sendCurrentSceneRadiusEdited();
            sceneRadiusLayer->setVisible(true);
        }

        mc->updateRequestNew();
        if(!(QObject::sender() == this->backend))
        {
            emit sendSelectedStates(states);
        }
        return;
    }

    mc->setMouseMode(MapControl::Panning);
    if(currentScene)
    {
        currentScene->deselectAllStates();
        if(editArea)
            currentScene->setAreaType(currentScene->getAreaType());
    }

    currentEntityPoint = nullptr;
    currentLandmarkPoint = nullptr;
    currentStatePoint = nullptr;
    currentScenePoint = nullptr;
    editAreaRadius = false;
    editAreaHeight = false;
    editAreaWidth = false;
    editAreaCorner = false;
    editArea = false;
    editSceneArea = false;
    editStateArea = false;
    areaCornerIndex2Edit = -1;
    writeUndo();
}

void RwaMapView::receiveSceneName(QString name)
{
    emit sendSceneName(currentScene, name);
}

void RwaMapView::receiveUpdateCurrentStateRadius()
{
    if(stateRadiusVisible)
        redrawStateRadii();
}

void RwaMapView::receiveHeroPositionEdited()
{
    redrawEntities();
}

void RwaMapView::receiveUpdateCurrentSceneRadius()
{
    if(sceneRadiusVisible)
        redrawSceneRadii();
}

void RwaMapView::setCurrentAsset(RwaAsset1 *asset)
{
    if(!asset)
        return;

    RwaGraphicsView::setCurrentAsset(asset);
}

void RwaMapView::setCurrentState(qint32 stateNumber)
{
    if(!currentScene)
        return;
    if(currentScene->getStates().empty())
        return;

    auto statesList = currentScene->getStates().begin();
    std::advance(statesList, stateNumber);

    RwaState *state = *statesList;
    setCurrentState(state);
}

void RwaMapView::setCurrentState(RwaState *state)
{
    RwaGraphicsView::setCurrentState(state);
}

void RwaMapView::setCurrentSceneWithoutRepositioning(RwaScene *scene)
{
    if(!scene)
        return;

    QDockWidget *window = static_cast<QDockWidget *>(parent());
    window->setWindowTitle("Map View - "+ QString::fromStdString(scene->objectName()));
    RwaGraphicsView::setCurrentScene(scene);
}

void RwaMapView::setCurrentScene(RwaScene *scene)
{
    if(!scene)
        return;

    QDockWidget *window = static_cast<QDockWidget *>(parent());
    window->setWindowTitle("Map View - "+ QString::fromStdString(scene->objectName()));

    if(!backend->isSimulationRunning())
    {
        setMapCoordinates(scene->currentViewCoordinates[0], scene->currentViewCoordinates[1]);
        setMap2AreaZoomLevel(scene);
    }

    RwaGraphicsView::setCurrentScene(scene);
}

void RwaMapView::moveCurrentAsset()
{
    if(assetsVisible)
        redrawAssets();
}
