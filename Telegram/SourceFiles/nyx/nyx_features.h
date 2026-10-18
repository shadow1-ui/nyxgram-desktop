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
void SetNumberPolicy(quint64 owner, const QString &key, int value);
void TransferSettings(QWidget *parent, Main::Session *session, bool import);
void ResetFeatureSettings(quint64 owner);
void ShowSavedProfiles(QWidget *parent, Main::Session *session);
void ChangeArchivePin(QWidget *parent, Main::Session *session);
void DisableArchivePin(QWidget *parent, Main::Session *session);
void ShowProfileMenu(PeerData *peer);
QString ProfileId(PeerData *peer);
QString ProfileRow(PeerData *peer);
// Returns true only when no prompt is needed. Otherwise the guarded callback is
// invoked once, asynchronously, only after an explicit native Send action.
bool ConfirmMedia(DocumentData *document, quint64 peer, bool approved, Fn<void()> confirmed, Fn<void()> cancelled=nullptr);
void ShowNotice(const QString &text);
void AskConfirmation(quint64 owner, const QString &title, const QString &text,
 const QString &confirmText, Fn<void()> confirmed);
bool ConfirmCall(quint64 owner, const QString &name, bool video, bool approved, Fn<void()> confirmed);
void ApplyPerformance();
void ApplySessionFeatures(Main::Session *session);
bool ArchiveAllowed(quint64 owner);
bool BlurDisabled();
int NumberPolicy(quint64 owner,const QString &key,int fallback=0);
int StartupFolder(Main::Session *session);
void RequestArchiveAccess(Main::Session *session, Fn<void()> done);
void LockArchive();
void CheckDesktopUpdates(bool manual=false);
}
