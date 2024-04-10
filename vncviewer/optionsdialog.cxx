#include "optionsdialog.h"

#include "appmanager.h"
#include "options/compressiontab.h"
#include "options/displaytab.h"
#include "options/inputtab.h"
#include "options/misctab.h"
#include "options/securitytab.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QStyledItemDelegate>

class OptionsDelegate : public QStyledItemDelegate
{
public:
  explicit OptionsDelegate(QObject *parent = nullptr)
    : QStyledItemDelegate(parent)
  {

  }

  QSize sizeHint (const QStyleOptionViewItem & option, const QModelIndex & index ) const
  {
    return QSize(100, 40);
  }
};

OptionsDialog::OptionsDialog(bool staysOnTop, QWidget* parent)
  : QDialog{parent}
{
  setWindowTitle(tr("TigerVNC Options"));
  setWindowFlag(Qt::WindowStaysOnTopHint, staysOnTop);

  QVBoxLayout* layout = new QVBoxLayout;
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);

  QHBoxLayout* hLayout = new QHBoxLayout;

  QListWidget* listWidget = new QListWidget;
  listWidget->setFrameShape(QFrame::NoFrame);
  listWidget->setItemDelegate(new OptionsDelegate(this));
  QStringList tabs = {tr("Compression"),
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
                      tr("Security"),
#endif
                      tr("Input"),
                      tr("Display"),
                      tr("Misc")};
  listWidget->addItems(tabs);
  listWidget->setCurrentRow(0);

  hLayout->addWidget(listWidget);

  QFrame* vFrame = new QFrame;
  vFrame->setFrameShape(QFrame::VLine);
  hLayout->addWidget(vFrame);

  tabWidget = new QStackedWidget;
  tabWidget->addWidget(new CompressionTab);
#if defined(HAVE_GNUTLS) || defined(HAVE_NETTLE)
  tabWidget->addWidget(new SecurityTab);
#endif
  tabWidget->addWidget(new InputTab);
  tabWidget->addWidget(new DisplayTab);
  tabWidget->addWidget(new MiscTab);

  hLayout->addWidget(tabWidget, 1);

  connect(listWidget, &QListWidget::currentRowChanged, tabWidget, &QStackedWidget::setCurrentIndex);

  layout->addLayout(hLayout);

  QFrame* hFrame = new QFrame;
  hFrame->setFrameShape(QFrame::HLine);
  layout->addWidget(hFrame);

  QHBoxLayout* btnsLayout = new QHBoxLayout;
  btnsLayout->setContentsMargins(10,10,10,10);
  btnsLayout->addStretch(1);
  QPushButton* applyBtn = new QPushButton(tr("Apply"));
  btnsLayout->addWidget(applyBtn, 0, Qt::AlignRight);
  QPushButton* closeBtn = new QPushButton(tr("Close"));
  btnsLayout->addWidget(closeBtn, 0, Qt::AlignRight);
  layout->addLayout(btnsLayout);

  setLayout(layout);

  setMinimumSize(600, 600);

  reset();

  connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
  connect(applyBtn, &QPushButton::clicked, this, &OptionsDialog::apply);
}

void OptionsDialog::apply()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
    if (w) {
      w->apply();
    }
  }
  AppManager::instance()->handleOptions();
  close();
}

void OptionsDialog::reset()
{
  for (int i = 0; i < tabWidget->count(); ++i) {
    auto w = qobject_cast<TabElement*>(tabWidget->widget(i));
    if (w) {
      w->reset();
    }
  }
}
