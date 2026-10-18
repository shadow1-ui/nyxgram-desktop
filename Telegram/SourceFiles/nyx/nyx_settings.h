#pragma once
#include <QtCore/QString>
#include <QtGui/QColor>
#include <QtCore/QStringList>
#include <QtCore/QtGlobal>
namespace Nyxgram {
bool LocalPremium(quint64 owner=0);
bool KeepDeletedMessages(quint64 owner=0);
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
QStringList IconKeys();
QStringList IconTitles();
void SetIconKey(const QString &key);
int DeletedMarker();
void SetDeletedMarker(int index);
void SetDeletedColor(const QString &color);
bool StartupEnabled();
void SetStartupEnabled(bool enabled);
bool ApplyNyxTheme();
void RestoreNightTheme();
void RefreshLocalProfile();
void ShowSettings();
void ShowStartup();
}
