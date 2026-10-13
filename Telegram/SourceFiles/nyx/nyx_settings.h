#pragma once
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtCore/QStringList>
#include <QtCore/QtGlobal>
namespace Nyxgram {
bool LocalPremium();
bool KeepDeletedMessages();
QColor DeletedColor();
QString DeletedSymbol();
void SaveLocalStatus(quint64 owner,quint64 document,int until);
quint64 LocalStatusDocument(quint64 owner);
QString IconKey();
QString IconPath();
QString ShortcutIconPath();
QStringList LocalPins(quint64 owner);
int LocalPinRank(quint64 owner, quint64 peer);
bool ToggleLocalPin(quint64 owner, quint64 peer);
void ShowSettings();
void ShowStartup();
}
