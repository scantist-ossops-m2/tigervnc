#ifndef CONTEXTMENUTRANSLATIONS_H
#define CONTEXTMENUTRANSLATIONS_H

#include "i18n.h"
#include "parameters.h"

#include <QObject>

class ContextMenuActions : public QObject
{
  Q_OBJECT
  Q_PROPERTY(QString disconnectAction MEMBER disconnectAction CONSTANT)
  Q_PROPERTY(QString fullScreenAction MEMBER fullScreenAction CONSTANT)
  Q_PROPERTY(QString minimizeAction MEMBER minimizeAction CONSTANT)
  Q_PROPERTY(QString revertSizeAction MEMBER revertSizeAction CONSTANT)
  Q_PROPERTY(QString ctrlKeyToggleAction MEMBER ctrlKeyToggleAction CONSTANT)
  Q_PROPERTY(QString altKeyToggleAction MEMBER altKeyToggleAction CONSTANT)
  Q_PROPERTY(QString menuKeyAction MEMBER menuKeyAction CONSTANT)
  Q_PROPERTY(QString ctrlAltDelAction MEMBER ctrlAltDelAction CONSTANT)
  Q_PROPERTY(QString refreshAction MEMBER refreshAction CONSTANT)
  Q_PROPERTY(QString optionDialogAction MEMBER optionDialogAction CONSTANT)
  Q_PROPERTY(QString infoDialogAction MEMBER infoDialogAction CONSTANT)
  Q_PROPERTY(QString aboutDialogAction MEMBER aboutDialogAction CONSTANT)

public:
  ContextMenuActions(QObject* parent = nullptr);

public slots:
  void disconnect();
  void minimize();
  void revertSize();
  void refresh();

private:
  QString disconnectAction    = p_("ContextMenu|", "Dis&connect");
  QString fullScreenAction    = p_("ContextMenu|", "&Full screen");
  QString minimizeAction      = p_("ContextMenu|", "Minimi&ze");
  QString revertSizeAction    = p_("ContextMenu|", "Resize &window to session");
  QString ctrlKeyToggleAction = p_("ContextMenu|", "&Ctrl");
  QString altKeyToggleAction  = p_("ContextMenu|", "&Alt");
  QString menuKeyAction =
      QString::asprintf(p_("ContextMenu|", "Send %s"), ViewerConfig::config()->menuKey().toStdString().c_str());
  QString ctrlAltDelAction   = p_("ContextMenu|", "Send Ctrl-Alt-&Del");
  QString refreshAction      = p_("ContextMenu|", "&Refresh screen");
  QString optionDialogAction = p_("ContextMenu|", "&Options...");
  QString infoDialogAction   = p_("ContextMenu|", "Connection &info...");
  QString aboutDialogAction  = p_("ContextMenu|", "About &TigerVNC viewer...");
};

#endif // CONTEXTMENUTRANSLATIONS_H
