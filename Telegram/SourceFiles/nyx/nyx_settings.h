#pragma once
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QtGlobal>
namespace Nyxgram {
QString IconKey();
QString IconPath();
QString ShortcutIconPath();
QStringList LocalPins(quint64 owner);
int LocalPinRank(quint64 owner, quint64 peer);
bool ToggleLocalPin(quint64 owner, quint64 peer);
void ShowSettings();
void ShowStartup();
}
