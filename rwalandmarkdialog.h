/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * License: MIT
 *
 * rwalandmarkdialog.h
 *
 * Name and description of a landmark, with the recorded position and how
 * it was captured. Used when recording (Delete button hidden), and when editing.
 */

#ifndef RWALANDMARKDIALOG_H
#define RWALANDMARKDIALOG_H

#include <QDialog>
#include "rwalandmark.h"

class QLineEdit;
class QPlainTextEdit;

class RwaLandmarkDialog : public QDialog
{
    Q_OBJECT
public:
    enum Result { Cancelled, Accepted, Deleted };

    RwaLandmarkDialog(QWidget *parent, const RwaLandmark *landmark, bool isNew);

    QString name() const;
    QString description() const;

    /** Modal. Applies name and description to the landmark on Accepted. */
    static Result edit(QWidget *parent, RwaLandmark *landmark, bool isNew);

    static QString captureText(const RwaLandmark *landmark);

private:
    QLineEdit *nameEdit;
    QPlainTextEdit *descriptionEdit;
    bool deleteRequested = false;
};

#endif // RWALANDMARKDIALOG_H
