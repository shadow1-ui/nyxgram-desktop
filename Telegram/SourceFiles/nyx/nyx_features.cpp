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
#include "settings/cloud_password/settings_cloud_password_common.h"
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
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
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
#include <QtCore/QDir>
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
Window::SessionController *FeatureController(Main::Session *session) {
 if (!session || ActiveFeatureSession()!=session || Core::App().passcodeLocked()) return nullptr;
 const auto window=Core::App().activeWindow();
 const auto controller=window?window->sessionController():nullptr;
 return controller && &controller->session()==session ? controller : nullptr;
}
void GuardNativeLayer(not_null<Ui::GenericBox*> box, Main::Session *session) {
 const auto guard=base::make_weak(session); const auto timer=new QTimer(box);
 QObject::connect(timer,&QTimer::timeout,box,[=] {
  if (!guard || !FeatureController(guard.get())) box->closeBox();
 });timer->start(100);
}
void NativeAction(not_null<Ui::GenericBox*> box, const QString &label, Fn<void()> done) {
 box->addRow(object_ptr<Ui::SettingsButton>(box, rpl::single(label), st::settingsButtonNoIcon), QMargins())
  ->addClickHandler(std::move(done));
}
QByteArray PinHash(const QString &pin,const QByteArray &salt){return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,pin.toUtf8(),salt,210000,32);}
bool Equal(const QByteArray&a,const QByteArray&b){if(a.size()!=b.size())return false;unsigned diff=0;for(int i=0;i<a.size();++i)diff|=uchar(a[i])^uchar(b[i]);return diff==0;}
void Transfer(QWidget *parent,quint64 owner,bool import) {
 const auto session=ActiveFeatureSession();
 if (!session || session->uniqueId()!=owner || !FeatureController(session)) return;
 const auto guard=base::make_weak(session);
 if (!import) {
  QJsonObject flags; for (const auto &key:ExportKeys()) flags[key]=Policy(owner,key,key=="confirmStickers"||key=="confirmGifs"||key=="showId"||key=="showDc");
  const auto path=QFileDialog::getSaveFileName(parent,QString::fromUtf8("Экспорт настроек"),"nyxgram-settings.json","JSON (*.json)");
  if (path.isEmpty() || !guard || !FeatureController(guard.get())) return;
  QJsonObject root; root["schemaVersion"]=1;root["platform"]="windows";root["flags"]=flags;
  const auto bytes=QJsonDocument(root).toJson(); QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) ShowNotice(QString::fromUtf8("Не удалось сохранить настройки."));
  return;
 }
 const auto path=QFileDialog::getOpenFileName(parent,QString::fromUtf8("Импорт настроек"),{},"JSON (*.json)");
 if (path.isEmpty() || !guard || !FeatureController(guard.get())) return;
 QFile file(path); if (file.size()>65536 || !file.open(QIODevice::ReadOnly)) { ShowNotice(QString::fromUtf8("Не удалось открыть файл настроек.")); return; }
 QJsonParseError error; const auto root=QJsonDocument::fromJson(file.readAll(),&error).object();
 if (error.error!=QJsonParseError::NoError || root["schemaVersion"].toInt()!=1 || !root["flags"].isObject()) { ShowNotice(QString::fromUtf8("Неподдерживаемый файл настроек.")); return; }
 const auto flags=root["flags"].toObject();
 for (const auto &key:ExportKeys()) if (flags.contains(key) && !flags[key].isBool()) { ShowNotice(QString::fromUtf8("Некорректные значения настроек.")); return; }
 AskConfirmation(owner,QString::fromUtf8("Импорт настроек"),QString::fromUtf8("Применить параметры интерфейса и приватности? Сообщения, PIN и данные входа не импортируются."),QString::fromUtf8("Применить"),[=] {
  if (!guard || !FeatureController(guard.get())) return;
  for (const auto &key:ExportKeys()) if (flags.contains(key)) SetPolicy(owner,key,flags[key].toBool());
  ApplyPerformance();
 });
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
void ShowNotice(const QString &text) {
 if (const auto controller=FeatureController(ActiveFeatureSession())) controller->show(Ui::MakeInformBox(text));
}
void AskConfirmation(quint64 owner, const QString &title, const QString &text,
 const QString &confirmText, Fn<void()> confirmed) {
 const auto session=ActiveFeatureSession();
 if (!session || session->uniqueId()!=owner) return;
 const auto controller=FeatureController(session); if (!controller) return;
 const auto guard=base::make_weak(session);
 auto box=Ui::MakeConfirmBox({
  .text=rpl::single(text),
  .confirmed=[=] { if (guard && FeatureController(guard.get())) confirmed(); },
  .confirmText=rpl::single(confirmText),
  .title=rpl::single(title),
 });
 GuardNativeLayer(box.data(),session);controller->show(std::move(box));
}
void ShowProfileMenu(PeerData *peer) {
 if (!peer) return;
 const auto session=&peer->session(); const auto controller=FeatureController(session); if (!controller) return;
 const auto guard=base::make_weak(session); const auto owner=session->uniqueId();
 const auto id=ProfileId(peer), username=peer->username(), raw=QString::number(peer->id.value);
 controller->show(Box([=](not_null<Ui::GenericBox*> box) {
  GuardNativeLayer(box,session);
  box->setTitle(rpl::single(peer->name()));
  const auto copy=[=](const QString &label,const QString &value) {
   NativeAction(box,label,[=] { if (guard && FeatureController(guard.get())) QGuiApplication::clipboard()->setText(value); box->closeBox(); });
  };
  NativeAction(box,QString::fromUtf8("Открыть профиль"),[=] { box->closeBox(); if (guard) if (const auto c=FeatureController(guard.get())) c->showPeerInfo(peer); });
  copy(QString::fromUtf8("Скопировать ID"),id);
  if (!username.isEmpty()) {
   copy(QString::fromUtf8("Скопировать имя пользователя"),"@"+username);
   copy(QString::fromUtf8("Скопировать публичную ссылку"),"https://t.me/"+username);
  }
  if (peer->isUser()) copy(QString::fromUtf8("Скопировать ссылку по ID"),"tg://openmessage?user_id="+id+"&nyxprofile=1");
  const auto saved=Store().value(Key(owner,"profiles")).toStringList().contains(raw);
  NativeAction(box,QString::fromUtf8(saved?"Удалить из закладок":"Сохранить в закладки"),[=] {
   if (!guard || !FeatureController(guard.get())) return;
   auto pins=Store().value(Key(owner,"profiles")).toStringList();pins.removeAll(raw);
   if (!saved) pins.prepend(raw);while (pins.size()>200) pins.removeLast();
   Store().setValue(Key(owner,"profiles"),pins);Store().sync();box->closeBox();
  });
  box->addButton(rpl::single(QString::fromUtf8("Закрыть")),[=] { box->closeBox(); });
 }));
}
bool ConfirmMedia(DocumentData *document,quint64 peer,bool approved,Fn<void()> confirmed,Fn<void()> cancelled) {
 if (approved || !document || (!document->sticker() && !document->isGifv())) return true;
 auto &session=document->session(); const auto owner=session.uniqueId();
 if (!Policy(owner,document->sticker()?"confirmStickers":"confirmGifs",true)) return true;
 const auto controller=FeatureController(&session); if (!controller || !peer) { if (cancelled) cancelled(); return false; }
 const auto guard=base::make_weak(&session); const auto controllerGuard=base::make_weak(controller);
 const auto target=session.data().peer(PeerId(peer));
 controller->show(Box([=](not_null<Ui::GenericBox*> box) {
  box->setTitle(rpl::single(QString::fromUtf8(document->sticker()?"Отправить стикер?":"Отправить GIF?")));
  box->addRow(object_ptr<Ui::FlatLabel>(box,rpl::single(target->name()),st::boxLabel));
  auto media=object_ptr<Window::MediaPreviewWidget>(box,controller,true);
  media->resize(st::boxWidth,style::ConvertScale(280));media->hide();
  const auto preview=box->addRow(std::move(media),QMargins());
  preview->showPreview(document->stickerOrGifOrigin(),document);
  const auto previewGuard=QPointer<Window::MediaPreviewWidget>(preview);
  const auto boxGuard=QPointer<Ui::GenericBox>(box.get());
  controller->lifetime().add([=] { if (previewGuard) delete previewGuard.data(); if (boxGuard) boxGuard->closeBox(); });
  const auto sent=box->lifetime().make_state<bool>(false);
  box->boxClosing() | rpl::start_with_next([=] {
   if (!*sent && cancelled) cancelled();
  },box->lifetime());
  box->addButton(rpl::single(QString::fromUtf8("Отправить")),[=] {
   if (*sent || !guard || !controllerGuard || !FeatureController(guard.get())) return;
   *sent=true;
   crl::on_main(controller,[=] { if (guard && controllerGuard && FeatureController(guard.get())) confirmed(); });
   box->closeBox();
  });
  box->addButton(rpl::single(QString::fromUtf8("Отмена")),[=] { box->closeBox(); });
  const auto timer=new QTimer(box);
  QObject::connect(timer,&QTimer::timeout,box,[=] {
   if (!guard || !controllerGuard || !FeatureController(guard.get())) box->closeBox();
  });timer->start(100);
 }));
 return false;
}
bool ConfirmCall(quint64 owner,const QString &name,bool video,bool approved,Fn<void()> confirmed) {
 if (approved || !Policy(owner,"confirmCalls")) return true;
 AskConfirmation(owner,QString::fromUtf8(video?"Видеозвонок":"Звонок"),
  QString::fromUtf8("Позвонить %1?").arg(name),QString::fromUtf8("Позвонить"),std::move(confirmed));
 return false;
}
void ApplyPerformance(){const auto s=ActiveFeatureSession();if(!s)return;const auto owner=s->uniqueId();currentFeatureOwner=owner;using namespace PowerSaving;auto flags=Current();const auto original=QString("originalPowerBaseline");if(!Store().contains(original))Store().setValue(original,int(flags.value()));flags=Flags::from_raw(Store().value(original).toUInt());if(Policy(owner,"noAnimations"))flags|=kAnimations|kChatBackground|kChatEffects;if(Policy(owner,"noEmoji"))flags|=kEmojiPanel|kEmojiChat|kEmojiReactions|kEmojiStatus;if(Policy(owner,"noStickers"))flags|=kStickersPanel|kStickersChat;Set(flags);}
bool ArchiveAllowed(quint64 owner){return !Core::App().passcodeLocked()&&(!Policy(owner,"protectArchive")||unlocked.value(owner,0)>crl::now());}
void LockArchive(){unlocked.clear();HideLockedSessions();}
namespace {
Window::SessionController *ArchiveController(Main::Session *session) { return FeatureController(session); }
void NativePinBox(Main::Session *session, const QString &title, bool repeat,
 Fn<bool(const QString&)> submit) {
 const auto controller=ArchiveController(session); if (!controller) return;
 const auto guard=base::make_weak(session);
 controller->show(Box([=](not_null<Ui::GenericBox*> box) {
  box->setTitle(rpl::single(title));
  // PasswordInput is a MaskedInputField, not RpWidget. Use Telegram's own
  // layout adapter instead of passing it directly to GenericBox::addRow.
  const auto pin=Settings::CloudPassword::AddPasswordField(box->verticalLayout(),
   rpl::single(QString::fromUtf8(repeat?"Новый PIN: 4–12 цифр":"PIN локальной истории")),QString()).get();
  const auto confirm=repeat?Settings::CloudPassword::AddPasswordField(box->verticalLayout(),
   rpl::single(QString::fromUtf8("Повторите PIN")),QString()).get():nullptr;
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
 Q_UNUSED(parent);
 const auto controller=FeatureController(session);if (!controller) return;
 const auto guard=base::make_weak(session);const auto owner=session->uniqueId();
 controller->show(Box([=](not_null<Ui::GenericBox*> box) {
  GuardNativeLayer(box,session);
  box->setTitle(rpl::single(QString::fromUtf8("Сохранённые профили")));
  const auto profiles=Store().value(Key(owner,"profiles")).toStringList();
  if (profiles.isEmpty()) box->addRow(object_ptr<Ui::FlatLabel>(box,rpl::single(QString::fromUtf8("Здесь появятся профили, добавленные в закладки.")),st::boxLabel));
  for (const auto &raw:profiles) {
   const auto peer=session->data().peer(PeerId(raw.toULongLong()));
   NativeAction(box,peer->name(),[=] { box->closeBox();if (guard && FeatureController(guard.get())) ShowProfileMenu(peer); });
  }
  box->addButton(rpl::single(QString::fromUtf8("Закрыть")),[=] { box->closeBox(); });
 }));
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
