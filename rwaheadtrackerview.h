/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * License: MIT
 *
 * rwaheadtrackerview.h
 *
 * Headtracker View: live numbers of the connected headtracker assembly (View >
 * Headtracker View). Bluetooth link, heading rate and jitter, the NTRIP
 * corrections loop and the RTK position. Polls RwaHeadtrackerConnect::stats()
 * at 1 Hz; the toggles that control the loop live in the Headtracker menu.
 */

#ifndef RWAHEADTRACKERVIEW_H
#define RWAHEADTRACKERVIEW_H

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include "rwaheadtrackerconnect.h"

class RwaHeadtrackerView : public QWidget
{
    Q_OBJECT
public:
    explicit RwaHeadtrackerView(QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QLabel *addRow(class QFormLayout *form, const QString &label);
    void setRow(QLabel *label, const QString &text, bool muted = false);

    RwaHeadtrackerConnect *headtracker;
    QTimer *timer;

    QLabel *bleState;
    QLabel *headingRate;
    QLabel *headingJitter;
    QLabel *headingValues;
    QLabel *correctionsState;
    QLabel *casterLabel;
    QLabel *ntripState;
    QLabel *rtcmBytes;
    QLabel *ggaLabel;
    QLabel *ntripError;
    QLabel *positionMode;
    QLabel *positionValue;
};

#endif // RWAHEADTRACKERVIEW_H
