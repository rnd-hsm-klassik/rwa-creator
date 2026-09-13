/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * Copyright (C) 2015 - 2022 Thomas Resch
 *
 * License: MIT
 *
 * rwamapview.h
 * by Thomas Resch
 *
 */

#ifndef RWAMAPVIEW_H
#define RWAMAPVIEW_H

#include "rwagraphicsview.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QKeyEvent>

class RwaMapView : public RwaGraphicsView
{
    Q_OBJECT
public:

    explicit RwaMapView(QWidget* parent = nullptr, RwaScene *scene = nullptr, QString name = "");
    ~RwaMapView();

    void mouseDownArrow(const QMouseEvent *event, const QPointF myPoint);
    void mouseDownRubber(const QMouseEvent *event, const QPointF myPoint);
    void mouseDownMarkee(const QMouseEvent *event, const QPointF myPoint);

    float selectRectX;
    float selectRectY;

    void moveCurrentState1(const QPointF myPoint);
    void moveCurrentScene(const QPointF myPoint);
    bool mouseDownScenes(const QPointF myPoint);

    void writeSettings();
private:
    int stateIndex2Delete;

    void readSettings();
public slots:
    void receiveMouseMoveEvent(const QMouseEvent*, const QPointF) override;
    void receiveMouseReleaseEvent() override;
    void receiveMouseDownEvent(const QMouseEvent*, const QPointF) override;
    void receiveSelectRect(QRectF selectRect);
    void receiveSceneName(QString name);
    void receiveUpdateCurrentStateRadius();
    void setCurrentSceneWithoutRepositioning(RwaScene *scene);
    //void receiveUpdateCurrentSceneRadius();

    bool mouseDownStates(const QPointF myPoint);
    bool mouseDownEntities(const QPointF myPoint);
    bool mouseDownLandmarks(const QPointF myPoint);

    /** Landmarks (flag button): a new landmark at the hero's position. */
    void recordLandmarkAtHero() override;
    void editLandmark(RwaLandmark *landmark);
    void deleteLandmark(RwaLandmark *landmark);
    void moveHeroToLandmark(RwaLandmark *landmark);
    void moveCurrentAssetToLandmark(RwaLandmark *landmark);

    void setMapCoordinates(double lon, double lat);
    void zoomIn() override;
    void zoomOut() override;
    void adaptSize(qint32 width, qint32 height) override;
    void moveCurrentAsset();
    void receiveUpdateCurrentSceneRadius();
    void receiveHeroPositionEdited();

protected:
     void keyPressEvent(QKeyEvent *event) override;
     RwaLandmark *landmarkAt(const QPointF myPoint, QmapPoint **point = nullptr);
     void selectLandmark(RwaLandmark *landmark);
     void showLandmarkMenu(RwaLandmark *landmark, const QPoint &globalPos);
     void setCurrentScene(RwaScene *scene) override;
     void setCurrentState(RwaState *state) override;
     void setCurrentState(qint32 stateNumber) override;
     void setCurrentAsset(RwaAsset1 *asset) override;

signals:
     void sendSceneName(RwaScene *scene, QString name);
     void sendMapCoordinates(double lon, double lat);
     void sendStateCoordinate(QPointF);
     void sendSelectedStates(QStringList states);
     void sendCurrentSceneWithoutRepositioning(RwaScene *scene);
};

#endif // RWAMAPVIEW_H
