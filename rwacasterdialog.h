/*
 * This file is part of the Rwa Creator.
 * An open-source cross-platform Middleware for creating interactive Soundwalks
 *
 * License: MIT
 *
 * rwacasterdialog.h
 *
 * Headtracker > NTRIP Caster...: host, port, mount point and the unit's
 * credentials. Edits an RwaCasterSettings; the caller saves.
 */

#ifndef RWACASTERDIALOG_H
#define RWACASTERDIALOG_H

#include <QDialog>
#include "rwacastersettings.h"

class QLineEdit;
class QSpinBox;

class RwaCasterDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RwaCasterDialog(QWidget *parent, const RwaCasterSettings &settings);
    RwaCasterSettings settings() const;

    /** Modal edit; true with the new values in settings when accepted. */
    static bool edit(QWidget *parent, RwaCasterSettings &settings);

private:
    QLineEdit *hostEdit;
    QSpinBox *portEdit;
    QLineEdit *mountEdit;
    QLineEdit *userEdit;
    QLineEdit *passEdit;
};

#endif // RWACASTERDIALOG_H
