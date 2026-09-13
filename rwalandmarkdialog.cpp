#include "rwalandmarkdialog.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

QString RwaLandmarkDialog::captureText(const RwaLandmark *l)
{
    QString text;
    if (l->source == "rtk") {
        switch (l->carrSoln) {
        case 2: text = tr("RTK fixed"); break;
        case 1: text = tr("RTK float"); break;
        default: text = tr("GNSS, no RTK"); break;
        }
        if (l->hAccMm > 0) {
            if (l->hAccMm < 1000)
                text += tr(", ±%1 cm").arg(l->hAccMm / 10.0, 0, 'f', 1);
            else
                text += tr(", ±%1 m").arg(l->hAccMm / 1000.0, 0, 'f', 2);
        }
    } else {
        text = tr("hero placed by hand");
    }
    if (!l->captured.empty())
        text += QStringLiteral(", %1").arg(QString::fromStdString(l->captured));
    return text;
}

RwaLandmarkDialog::RwaLandmarkDialog(QWidget *parent, const RwaLandmark *landmark, bool isNew)
    : QDialog(parent)
{
    setWindowTitle(isNew ? tr("New Landmark") : tr("Landmark"));
    setModal(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(form);

    nameEdit = new QLineEdit(QString::fromStdString(landmark->objectName()), this);
    nameEdit->selectAll();
    form->addRow(tr("Name:"), nameEdit);

    descriptionEdit = new QPlainTextEdit(QString::fromStdString(landmark->description), this);
    descriptionEdit->setPlaceholderText(tr("What is here, what does it sound like, why it matters..."));
    descriptionEdit->setFixedHeight(90);
    form->addRow(tr("Description:"), descriptionEdit);

    const std::vector<double> c = landmark->getCoordinates();
    QLabel *position = new QLabel(QStringLiteral("%1, %2")
                                  .arg(c.size() > 1 ? c[1] : 0.0, 0, 'f', 7)
                                  .arg(c.size() > 0 ? c[0] : 0.0, 0, 'f', 7), this);
    position->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Position:"), position);

    QLabel *capture = new QLabel(captureText(landmark), this);
    capture->setEnabled(false);
    form->addRow(tr("Recorded:"), capture);

    QHBoxLayout *buttons = new QHBoxLayout;
    if (!isNew) {
        QPushButton *deleteButton = new QPushButton(tr("Delete"), this);
        connect(deleteButton, &QPushButton::clicked, this, [this]() {
            deleteRequested = true;
            accept();
        });
        buttons->addWidget(deleteButton);
    }
    buttons->addStretch(1);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttons->addWidget(box);
    layout->addLayout(buttons);

    setMinimumWidth(400);
}

QString RwaLandmarkDialog::name() const
{
    return nameEdit->text().trimmed();
}

QString RwaLandmarkDialog::description() const
{
    return descriptionEdit->toPlainText();
}

RwaLandmarkDialog::Result RwaLandmarkDialog::edit(QWidget *parent, RwaLandmark *landmark, bool isNew)
{
    RwaLandmarkDialog dialog(parent, landmark, isNew);
    if (dialog.exec() != QDialog::Accepted)
        return Cancelled;
    if (dialog.deleteRequested)
        return Deleted;
    if (!dialog.name().isEmpty())
        landmark->setObjectName(dialog.name().toStdString());
    landmark->description = dialog.description().toStdString();
    return Accepted;
}
