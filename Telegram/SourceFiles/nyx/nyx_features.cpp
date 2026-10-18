// Nyxgram client-only controls. This module never changes server entitlements.
#include "nyx/nyx_features.h"
#include "nyx/nyx_settings.h"
#include "nyx/nyx_history.h"
#include "core/application.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_photo.h"
#include "data/data_chat_filters.h"
#include "base/options.h"
#include "ui/chat/chat_style_radius.h"
#include "settings/settings_chat.h"
#include "settings/settings_advanced.h"
#include "window/themes/window_theme.h"
#include "data/data_document.h"
#include "api/api_updates.h"
#include "apiwrap.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "window/window_media_preview.h"
#include "ui/power_saving.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/boxes/confirm_box.h"
#include "styles/style_boxes.h"
#include "settings.h"
#include <QtCore/QSettings>
#include <QtCore/QPointer>
#include <algorithm>
#include <vector>
#include <QtGui/QCursor>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QTimer>
#include <QtCore/QRandomGenerator>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtNetwork/QPasswordDigestor>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QDir>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
namespace Nyxgram {
namespace {
QSettings &Store(){static QSettings p(cWorkingDir()+QString("tdata/nyx/preferences.ini"),QSettings::IniFormat);return p;}
QString Key(quint64 owner,const QString &key){return QString("account/%1/%2").arg(owner).arg(key);}
QHash<quint64,qint64> unlocked;
quint64 currentFeatureOwner=0;
std::vector<base::weak_ptr<Main::Session>> featureSessions;
void HideLockedSessions(){for(auto i=featureSessions.begin();i!=featureSessions.end();){if(!*i){i=featureSessions.erase(i);continue;}if(!ArchiveAllowed((*i)->uniqueId()))HideDeletedCopies(i->get());++i;}}
QStringList ExportKeys(){return {"hideRead","hideOnline","hideTyping","storyGhost","hidePhone","confirmCalls","confirmStickers","confirmGifs","showId","showDc","noDownloads","noGifs","noVideos","noAnimations","noEmoji","noStickers","noBlur","stickerTime","gifTime","roundTime","avatars","dates"};}
void Note(QVBoxLayout *l,const QString &text){auto w=new QLabel(text);w->setWordWrap(true);l->addWidget(w);}
QByteArray PinHash(const QString &pin,const QByteArray &salt){return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,pin.toUtf8(),salt,210000,32);}
bool Equal(const QByteArray&a,const QByteArray&b){if(a.size()!=b.size())return false;unsigned diff=0;for(int i=0;i<a.size();++i)diff|=uchar(a[i])^uchar(b[i]);return diff==0;}
void Transfer(QWidget *parent,quint64 owner,bool import){
 if(!import){auto flags=QJsonObject();for(const auto &key:ExportKeys())flags[key]=Policy(owner,key,key=="confirmStickers"||key=="confirmGifs"||key=="showId"||key=="showDc");QJsonObject root;root["schemaVersion"]=1;root["platform"]="windows";root["flags"]=flags;const auto path=QFileDialog::getSaveFileName(parent,QString::fromUtf8("Экспорт настроек"),"nyxgram-settings.json","JSON (*.json)");if(path.isEmpty())return;QSaveFile file(path);if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(root).toJson());file.commit();}return;}
 const auto path=QFileDialog::getOpenFileName(parent,QString::fromUtf8("Импорт настроек"),{},"JSON (*.json)");if(path.isEmpty())return;QFile file(path);if(file.size()>65536||!file.open(QIODevice::ReadOnly))return;QJsonParseError error;const auto root=QJsonDocument::fromJson(file.readAll(),&error).object();if(error.error!=QJsonParseError::NoError||root["schemaVersion"].toInt()!=1||!root["flags"].isObject()){QMessageBox::warning(parent,"Nyxgram",QString::fromUtf8("Неподдерживаемый файл настроек."));return;}const auto flags=root["flags"].toObject();for(const auto &key:ExportKeys())if(flags.contains(key)&&!flags[key].isBool()){QMessageBox::warning(parent,"Nyxgram",QString::fromUtf8("Некорректные значения настроек."));return;}if(QMessageBox::question(parent,"Nyxgram",QString::fromUtf8("Применить параметры интерфейса и приватности? Сообщения, PIN и данные входа не импортируются."))!=QMessageBox::Yes)return;for(const auto &key:ExportKeys())if(flags.contains(key))SetPolicy(owner,key,flags[key].toBool());ApplyPerformance();
}
}
Main::Session *ActiveFeatureSession(){auto &d=Core::App().domain();return d.started()&&d.active().sessionExists()?&d.active().session():nullptr;}
bool Policy(quint64 owner,const QString &key,bool fallback){return owner?Store().value(Key(owner,key),fallback).toBool():fallback;}
void SetPolicy(quint64 owner,const QString &key,bool value){if(!owner)return;Store().setValue(Key(owner,key),value);Store().sync();if(key=="keepDeleted"&&!value)if(auto s=ActiveFeatureSession();s&&s->uniqueId()==owner)HideDeletedCopies(s);if(key=="hideOnline")if(auto s=ActiveFeatureSession();s&&s->uniqueId()==owner)s->api().updates().updateOnline();}
int NumberPolicy(quint64 owner,const QString &key,int fallback){return Store().value(Key(owner,key),fallback).toInt();}
bool BlurDisabled(){return Policy(currentFeatureOwner,"noBlur");}
int StartupFolder(Main::Session *session){if(!session)return 0;const auto id=NumberPolicy(session->uniqueId(),"startupFolder",0);for(const auto &f:session->data().chatsFilters().list())if(f.id()==id)return id;return session->data().chatsFilters().defaultId();}
QString ProfileId(PeerData *peer){if(!peer)return {};return peer->isUser()?QString::number(peerToUser(peer->id).bare):QString("-")+QString::number(peer->isChannel()?peerToChannel(peer->id).bare:peerToChat(peer->id).bare);}
QString ProfileRow(PeerData *peer){if(!peer)return {};const auto owner=peer->session().uniqueId();QStringList parts;if(Policy(owner,"showId",true))parts<<QString("ID: ")+ProfileId(peer);if(Policy(owner,"showDc",true)){const auto location=peer->userpicLocation();const auto storage=std::get_if<StorageFileLocation>(&location.file().data);const auto dc=storage?storage->dcId():0;parts<<QString("DC: ")+(dc?QString::number(dc):QString::fromUtf8("—"));}return parts.join("  ·  ");}
void ShowProfileMenu(PeerData *peer){
 if(!peer)return;const auto guard=base::make_weak(&peer->session());const auto owner=peer->session().uniqueId();const auto id=ProfileId(peer),username=peer->username();QMenu menu;auto copy=[&](const QString &name,const QString &value){menu.addAction(name,[value]{QGuiApplication::clipboard()->setText(value);});};copy(QString::fromUtf8("Скопировать ID"),id);if(!username.isEmpty()){copy(QString::fromUtf8("Скопировать имя пользователя"),"@"+username);copy(QString::fromUtf8("Скопировать публичную ссылку"),"https://t.me/"+username);}if(peer->isUser())copy(QString::fromUtf8("Скопировать ссылку по ID"),"tg://openmessage?user_id="+id+"&nyxprofile=1");menu.addAction(QString::fromUtf8("Сохранить профиль в закладки"),[=]{if(!guard)return;auto pins=Store().value(Key(owner,"profiles")).toStringList();const auto raw=QString::number(peer->id.value);pins.removeAll(raw);pins.prepend(raw);while(pins.size()>200)pins.removeLast();Store().setValue(Key(owner,"profiles"),pins);Store().sync();});menu.exec(QCursor::pos());
}
bool ConfirmMedia(DocumentData *document,quint64 peer,bool approved){
 if(approved||!document||(!document->sticker()&&!document->isGifv()))return true;auto &session=document->session();const auto owner=session.uniqueId();if(!Policy(owner,document->sticker()?"confirmStickers":"confirmGifs",true))return true;Q_UNUSED(peer);const auto guard=base::make_weak(&session);const auto window=Core::App().activeWindow();const auto controller=window?window->sessionController():nullptr;if(!controller||&controller->session()!=&session)return false;
 QDialog dialog;dialog.setWindowModality(Qt::ApplicationModal);dialog.setWindowTitle(QString::fromUtf8("Подтверждение отправки · Nyxgram"));auto layout=new QVBoxLayout(&dialog);QPointer<Window::MediaPreviewWidget> preview=new Window::MediaPreviewWidget(&dialog,controller);const auto controllerGuard=base::make_weak(controller);const auto dialogGuard=QPointer<QDialog>(&dialog);controller->lifetime().add([=]{if(preview)delete preview.data();if(dialogGuard)dialogGuard->reject();});auto ownerTimer=new QTimer(&dialog);QObject::connect(ownerTimer,&QTimer::timeout,&dialog,[=]{if(!guard||!controllerGuard||ActiveFeatureSession()!=guard.get())dialogGuard->reject();});ownerTimer->start(100);preview->setMinimumSize(320,320);layout->addWidget(preview);Note(layout,QString::fromUtf8("Отправить выбранное медиа? Отмена ничего не отправляет."));auto buttons=new QDialogButtonBox(&dialog);auto send=buttons->addButton(QString::fromUtf8("Отправить"),QDialogButtonBox::AcceptRole);buttons->addButton(QString::fromUtf8("Отмена"),QDialogButtonBox::RejectRole);QObject::connect(send,&QPushButton::clicked,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);preview->showPreview(document->stickerOrGifOrigin(),document);const auto ok=dialog.exec()==QDialog::Accepted;if(preview)preview->hidePreview();return ok&&guard&&controllerGuard&&ActiveFeatureSession()==guard.get();
}
bool ConfirmCall(quint64 owner,const QString &name,bool video){if(!Policy(owner,"confirmCalls"))return true;const auto result=QMessageBox::question(QApplication::activeWindow(),"Nyxgram",QString::fromUtf8("Начать %1 с %2?").arg(video?QString::fromUtf8("видеозвонок"):QString::fromUtf8("звонок"),name));const auto s=ActiveFeatureSession();return result==QMessageBox::Yes&&s&&s->uniqueId()==owner;}
void ApplyPerformance(){const auto s=ActiveFeatureSession();if(!s)return;const auto owner=s->uniqueId();currentFeatureOwner=owner;using namespace PowerSaving;auto flags=Current();const auto original=QString("originalPowerBaseline");if(!Store().contains(original))Store().setValue(original,int(flags.value()));flags=Flags::from_raw(Store().value(original).toUInt());if(Policy(owner,"noAnimations"))flags|=kAnimations|kChatBackground|kChatEffects;if(Policy(owner,"noEmoji"))flags|=kEmojiPanel|kEmojiChat|kEmojiReactions|kEmojiStatus;if(Policy(owner,"noStickers"))flags|=kStickersPanel|kStickersChat;Set(flags);}
bool ArchiveAllowed(quint64 owner){return !Core::App().passcodeLocked()&&(!Policy(owner,"protectArchive")||unlocked.value(owner,0)>crl::now());}
void LockArchive(){unlocked.clear();HideLockedSessions();}
namespace {
Window::SessionController *ArchiveController(Main::Session *session) {
 if (!session || ActiveFeatureSession()!=session || Core::App().passcodeLocked()) return nullptr;
 const auto window=Core::App().activeWindow();
 const auto controller=window?window->sessionController():nullptr;
 return controller && &controller->session()==session ? controller : nullptr;
}
void NativePinBox(Main::Session *session, const QString &title, bool repeat,
 Fn<bool(const QString&)> submit) {
 const auto controller=ArchiveController(session); if (!controller) return;
 const auto guard=base::make_weak(session);
 controller->show(Box([=](not_null<Ui::GenericBox*> box) {
  box->setTitle(rpl::single(title));
  const auto pin=box->addRow(object_ptr<Ui::PasswordInput>(box,st::defaultInputField,
   rpl::single(QString::fromUtf8(repeat?"Новый PIN: 4–12 цифр":"PIN локальной истории"))));
  const auto confirm=repeat?box->addRow(object_ptr<Ui::PasswordInput>(box,st::defaultInputField,
   rpl::single(QString::fromUtf8("Повторите PIN")))):nullptr;
  const auto save=[=] {
   if (!guard || !ArchiveController(guard.get())) { box->closeBox(); return; }
   const auto value=pin->text();
   if (confirm && (value.size()<4 || value.size()>12
       || !std::all_of(value.begin(),value.end(),[](QChar c){return c>='0'&&c<='9';})
       || value!=confirm->text())) { pin->showError(); confirm->showError(); return; }
   if (submit(value)) box->closeBox(); else pin->showError();
  };
  box->addButton(rpl::single(QString::fromUtf8(repeat?"Сохранить":"Открыть")),save);
  box->addButton(rpl::single(QString::fromUtf8("Отмена")),[=] { box->closeBox(); });
  QObject::connect(pin,&Ui::MaskedInputField::submitted,box,[=] { save(); });
  if (confirm) QObject::connect(confirm,&Ui::MaskedInputField::submitted,box,[=] { save(); });
  box->setFocusCallback([=] { pin->setFocus(); });
  const auto timer=new QTimer(box);
  QObject::connect(timer,&QTimer::timeout,box,[=] {
   if (!guard || !ArchiveController(guard.get()) || QGuiApplication::applicationState()!=Qt::ApplicationActive) box->closeBox();
  });
  timer->start(100);
 }));
}
}
void RequestArchiveAccess(Main::Session *session, Fn<void()> done) {
 if (!ArchiveController(session)) return;
 const auto owner=session->uniqueId(); const auto guard=base::make_weak(session);
 if (ArchiveAllowed(owner)) { done(); return; }
 if (Store().value(Key(owner,"pinRetry"),0).toLongLong()>QDateTime::currentSecsSinceEpoch()) {
  ArchiveController(session)->show(Ui::MakeInformBox(QString::fromUtf8("Слишком много попыток. Подождите минуту."))); return;
 }
 NativePinBox(session,QString::fromUtf8("Открыть локальную историю"),false,[=](const QString &pin) {
  if (!guard || !ArchiveController(guard.get())) return false;
  const auto now=QDateTime::currentSecsSinceEpoch();
  if (Store().value(Key(owner,"pinRetry"),0).toLongLong()>now) return false;
  const auto salt=QByteArray::fromBase64(Store().value(Key(owner,"pinSalt")).toByteArray());
  const auto hash=QByteArray::fromBase64(Store().value(Key(owner,"pinHash")).toByteArray());
  if (hash.size()!=32 || salt.size()!=24 || !Equal(PinHash(pin,salt),hash)) {
   const auto fails=Store().value(Key(owner,"pinFailures"),0).toInt()+1;
   Store().setValue(Key(owner,"pinFailures"),fails);
   if (fails>=5) Store().setValue(Key(owner,"pinRetry"),now+60);
   Store().sync(); return false;
  }
  Store().setValue(Key(owner,"pinFailures"),0); Store().remove(Key(owner,"pinRetry")); Store().sync();
  unlocked[owner]=crl::now()+5*60*1000;
  // Run after closing the PIN layer, never through a nested blocking event loop.
  crl::on_main(session,[=] { if (guard && ArchiveController(guard.get()) && ArchiveAllowed(owner)) done(); });
  return true;
 });
}
void ApplySessionFeatures(Main::Session *s){if(!s)return;if(std::none_of(featureSessions.begin(),featureSessions.end(),[=](const auto &weak){return weak.get()==s;}))featureSessions.push_back(base::make_weak(s));ApplyPerformance();if(!ArchiveAllowed(s->uniqueId()))HideDeletedCopies(s);static bool connected=false;if(!connected){connected=true;QObject::connect(qApp,&QGuiApplication::applicationStateChanged,[](Qt::ApplicationState state){if(state!=Qt::ApplicationActive)LockArchive();});auto timer=new QTimer(qApp);QObject::connect(timer,&QTimer::timeout,[]{HideLockedSessions();});timer->start(1000);}CheckDesktopUpdates(false);}
void SetNumberPolicy(quint64 owner, const QString &key, int value) {
 if (!owner) return; Store().setValue(Key(owner,key),value); Store().sync();
}
void TransferSettings(QWidget *parent, Main::Session *session, bool import) {
 if (session && ActiveFeatureSession()==session) Transfer(parent,session->uniqueId(),import);
}
void ResetFeatureSettings(quint64 owner) {
 for (const auto &key:ExportKeys()) Store().remove(Key(owner,key));
 Store().sync(); ApplyPerformance();
}
void ShowFeatures(QWidget *parent) { Q_UNUSED(parent); ShowSettings(); }
void ShowSavedProfiles(QWidget *parent, Main::Session *session) {
 if (!session || ActiveFeatureSession()!=session) return;
 const auto owner=session->uniqueId(); const auto guard=base::make_weak(session);
 if(!guard)return;QMenu menu;for(const auto &raw:Store().value(Key(owner,"profiles")).toStringList()){const auto peer=guard->data().peer(PeerId(raw.toULongLong()));auto sub=menu.addMenu(peer->name());sub->addAction(QString::fromUtf8("Открыть профиль"),[=]{if(guard)if(auto w=Core::App().activeWindow();w&&w->sessionController()&& &w->sessionController()->session()==guard.get())w->sessionController()->showPeerInfo(peer);});sub->addAction(QString::fromUtf8("Скопировать ID"),[=]{if(guard)QGuiApplication::clipboard()->setText(ProfileId(peer));});sub->addAction(QString::fromUtf8("Удалить закладку"),[=]{auto pins=Store().value(Key(owner,"profiles")).toStringList();pins.removeAll(raw);Store().setValue(Key(owner,"profiles"),pins);Store().sync();});}menu.exec(QCursor::pos());
}
void ChangeArchivePin(QWidget *parent, Main::Session *session) {
 Q_UNUSED(parent);
 if (!ArchiveController(session)) return;
 const auto guard=base::make_weak(session); const auto owner=session->uniqueId();
 RequestArchiveAccess(session,[=] {
  if (!guard || !ArchiveController(guard.get())) return;
  NativePinBox(guard.get(),QString::fromUtf8("PIN локальной истории"),true,[=](const QString &pin) {
   if (!guard || !ArchiveController(guard.get()) || !ArchiveAllowed(owner)) return false;
   QByteArray salt(24,0); for (auto &c:salt) c=char(QRandomGenerator::system()->generate());
   Store().setValue(Key(owner,"pinSalt"),salt.toBase64());
   Store().setValue(Key(owner,"pinHash"),PinHash(pin,salt).toBase64());
   SetPolicy(owner,"protectArchive",true); unlocked[owner]=crl::now()+300000; return true;
  });
 });
}
void DisableArchivePin(QWidget *parent, Main::Session *session) {
 Q_UNUSED(parent);
 if (!ArchiveController(session)) return;
 const auto guard=base::make_weak(session); const auto owner=session->uniqueId();
 RequestArchiveAccess(session,[=] {
  if (!guard || !ArchiveController(guard.get()) || !ArchiveAllowed(owner)) return;
  SetPolicy(owner,"protectArchive",false); unlocked.remove(owner);
 });
}
}
