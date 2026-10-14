#pragma once
#include <QtCore/QString>
#include <QtCore/QtGlobal>
class QWidget; class PeerData; class DocumentData;
namespace Main { class Session; }
namespace Window { class SessionController; }
namespace Nyxgram {
bool Policy(quint64 owner, const QString &key, bool fallback=false);
void SetPolicy(quint64 owner, const QString &key, bool value);
Main::Session *ActiveFeatureSession();
void ShowFeatures(QWidget *parent=nullptr);
void ShowProfileMenu(PeerData *peer);
QString ProfileId(PeerData *peer);
QString ProfileRow(PeerData *peer);
bool ConfirmMedia(DocumentData *document, quint64 peer, bool approved=false);
bool ConfirmCall(quint64 owner, const QString &name, bool video);
void ApplyPerformance();
void ApplySessionFeatures(Main::Session *session);
bool ArchiveAllowed(quint64 owner);
bool BlurDisabled();
int NumberPolicy(quint64 owner,const QString &key,int fallback=0);
int StartupFolder(Main::Session *session);
bool UnlockArchive(Main::Session *session);
void LockArchive();
void CheckDesktopUpdates(bool manual=false);
}
