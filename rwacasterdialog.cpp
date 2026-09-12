#include "rwacasterdialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QDialogButtonBox>

RwaCasterDialog::RwaCasterDialog(QWidget *parent, const RwaCasterSettings &s)
    : QDialog(parent)
{
    setWindowTitle(tr("NTRIP Caster"));
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    hostEdit = new QLineEdit(s.host, this);
    hostEdit->setPlaceholderText(tr("caster host name or IP"));
    form->addRow(tr("Host:"), hostEdit);

    portEdit = new QSpinBox(this);
    portEdit->setRange(1, 65535);
    portEdit->setValue(s.port > 0 ? s.port : RwaCasterSettings::defaultPort);
    form->addRow(tr("Port:"), portEdit);

    mountEdit = new QLineEdit(s.mount, this);
    mountEdit->setPlaceholderText(tr("mount point, without leading slash"));
    form->addRow(tr("Mount point:"), mountEdit);

    userEdit = new QLineEdit(s.user, this);
    userEdit->setPlaceholderText(tr("this unit's caster account"));
    form->addRow(tr("Username:"), userEdit);

    passEdit = new QLineEdit(s.pass, this);
    passEdit->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password:"), passEdit);

    QLabel *note = new QLabel(tr(
        "The username belongs to one headtracker unit. Caster accounts are single-session: "
        "while RWA Creator holds the session for a unit, the RWA Player using the same "
        "username gets no corrections, and vice versa. The session runs only while that "
        "unit is connected over Bluetooth and <i>NTRIP Corrections</i> is enabled."), this);
    note->setWordWrap(true);
    note->setTextFormat(Qt::RichText);
    layout->addWidget(note);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    setMinimumWidth(420);
}

RwaCasterSettings RwaCasterDialog::settings() const
{
    RwaCasterSettings s;
    s.host = hostEdit->text().trimmed();
    s.port = quint16(portEdit->value());
    s.mount = RwaCasterSettings::mountPoint(mountEdit->text());
    s.user = userEdit->text().trimmed();
    s.pass = passEdit->text();
    return s;
}

bool RwaCasterDialog::edit(QWidget *parent, RwaCasterSettings &settings)
{
    RwaCasterDialog dialog(parent, settings);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    settings = dialog.settings();
    return true;
}
