// Nyxgram-only visual preferences and local ordering. No subscription or MTProto changes.
#include "nyx/nyx_settings.h"
#include "nyx/nyx_features.h"
#include "nyx/nyx_history.h"
#include "settings/settings_nyxgram.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_user.h"
#include "data/data_emoji_statuses.h"
#include "data/data_session.h"
#include "data/data_changes.h"
#include <QtCore/QDateTime>
#include "core/application.h"
#include "core/utils.h"
#include "window/themes/window_theme.h"
#include "ui/effects/animations.h"
#ifdef Q_OS_WIN
#include "platform/win/windows_app_user_model_id.h"
#endif
#include <QtCore/QSettings>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QDir>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <memory>
#include <QtGui/QScreen>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QSplashScreen>

namespace Nyxgram {
namespace {
const QStringList kIcons = { "original", "void", "aurora", "nebula", "eclipse", "lunar", "midnight", "ember", "frost", "rose", "jade", "solar", "orbit", "prism", "obsidian", "paper" };
const QStringList kTitles = { "Nyxgram", "Void", "Aurora", "Nebula", "Eclipse", "Lunar", "Midnight", "Ember", "Frost", "Rose", "Jade", "Solar", "Orbit", "Prism", "Obsidian", "Paper" };
QSettings &Prefs() {
 static QSettings prefs(cWorkingDir() + QString("tdata/nyx/preferences.ini"), QSettings::IniFormat);
 return prefs;
}
QString Materialize(const QString &source, const QString &name) {
 const auto dir = cWorkingDir() + QString("tdata/nyx/");
 if (!QDir().mkpath(dir)) return QString();
 QFile input(source);
 if (!input.open(QIODevice::ReadOnly)) return QString();
 const auto bytes = input.readAll();
 const auto dest = dir + name;
 QFile old(dest);
 if (old.open(QIODevice::ReadOnly) && old.readAll() == bytes) return dest;
 old.close();
 QSaveFile output(dest);
 if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) return QString();
 return dest;
}
struct Pins { QStringList ids; QHash<quint64,int> ranks; };
QHash<quint64, Pins> pinCache;
Pins &ReadPins(quint64 owner) {
 if (!pinCache.contains(owner)) {
  Pins state;
  const auto stored = Prefs().value(QString("pins/%1").arg(owner)).toStringList();
  for (const auto &value : stored) {
   bool ok = false;
   const auto id = value.toULongLong(&ok);
   if (ok && id && !state.ranks.contains(id) && state.ids.size() < 500) {
    state.ranks.insert(id, int(state.ids.size()));
    state.ids.push_back(QString::number(id));
   }
  }
  pinCache.insert(owner, state);
 }
 return pinCache[owner];
}
class Startup final : public QSplashScreen {
public:
 Startup() : QSplashScreen(QPixmap(600, 480)), background(":/nyx/startup.jpg"), logo(":/nyx/icons/original.png") {
  setWindowTitle("Nyxgram");
  setWindowIcon(QIcon(IconPath()));
  setAttribute(Qt::WA_DeleteOnClose);
  elapsed.start();
  auto timer = new QTimer(this);
  QObject::connect(timer, &QTimer::timeout, this, [this] {
   if (elapsed.elapsed() >= 680) close(); else update();
  });
  timer->start(16);
  QTimer::singleShot(900, this, [this] { close(); });
 }
protected:
 void drawContents(QPainter *p) override {
  p->setRenderHint(QPainter::Antialiasing);
  p->fillRect(rect(), QColor("#080d1a"));
  if (!background.isNull()) {
   const auto scaled = background.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
   p->drawPixmap((width()-scaled.width())/2, (height()-scaled.height())/2, scaled);
  }
  const auto center = QPointF(width()/2., height()*.40);
  const auto progress = qMin(1., elapsed.elapsed()/680.);
  const auto radius = 62. + 8.*qMin(1., progress/.45);
  QRadialGradient halo(center, radius*1.9);
  halo.setColorAt(0, QColor(156,116,235, Prefs().value("localPremium", false).toBool() ? 85 : 68));
  halo.setColorAt(1, QColor(112,217,232,0));
  p->setPen(Qt::NoPen); p->setBrush(halo); p->drawEllipse(center, radius*1.9, radius*1.9);
  p->setBrush(Qt::NoBrush);p->setPen(QPen(QColor("#70d9e8"),1));
  p->drawArc(QRectF(center.x()-radius-14,center.y()-radius-14,2*(radius+14),2*(radius+14)),int((-40+progress*35)*16),225*16);
  QPainterPath clip; clip.addEllipse(center,radius,radius); p->save(); p->setClipPath(clip);
  p->drawPixmap(QRectF(center.x()-radius,center.y()-radius,2*radius,2*radius),logo,QRectF(logo.rect()));p->restore();
  QFont title = p->font();title.setPixelSize(27);title.setBold(true);title.setLetterSpacing(QFont::AbsoluteSpacing,4);
  p->setFont(title);p->setPen(QColor("#f1f3fa"));p->drawText(QRectF(0,center.y()+radius+34,width(),45),Qt::AlignCenter,"NYXGRAM");
  title.setBold(false);title.setPixelSize(14);title.setLetterSpacing(QFont::AbsoluteSpacing,0);p->setFont(title);p->setPen(QColor("#b7bed2"));
  p->drawText(QRectF(0,center.y()+radius+78,width(),28),Qt::AlignCenter,QString::fromUtf8("Связь в Вашем ритме"));
 }
 void mousePressEvent(QMouseEvent *) override { close(); }
private:
 QPixmap background, logo;
 QElapsedTimer elapsed;
};
}
bool LocalPremium(quint64 owner){if(!owner){const auto s=ActiveFeatureSession();owner=s?s->uniqueId():0;}return owner&&Policy(owner,"localPremium",Prefs().value("localPremium",false).toBool());}
bool KeepDeletedMessages(quint64 owner){if(!owner)if(auto s=ActiveFeatureSession())owner=s->uniqueId();return Policy(owner,"keepDeleted",Prefs().value("keepDeleted",false).toBool());}
QColor DeletedColor(){return QColor(Prefs().value("deletedColor","#d976a6").toString());}
QString DeletedSymbol(){const QStringList symbols={QString::fromUtf8("✕"),QString::fromUtf8("⌫"),QString::fromUtf8("●"),QString::fromUtf8("▱")};return symbols[qBound(0,Prefs().value("deletedIcon",0).toInt(),3)];}
void SaveLocalStatus(quint64 owner,quint64 document,int until){Prefs().setValue(QString("status/%1/doc").arg(owner),QString::number(document));Prefs().setValue(QString("status/%1/until").arg(owner),until);Prefs().sync();}
quint64 LocalStatusDocument(quint64 owner){const auto until=Prefs().value(QString("status/%1/until").arg(owner),0).toLongLong();return until&&until<=QDateTime::currentSecsSinceEpoch()?0:Prefs().value(QString("status/%1/doc").arg(owner),"0").toString().toULongLong();}
Main::Session *ActiveSession(){auto &d=Core::App().domain();return d.started()&&d.active().sessionExists()?&d.active().session():nullptr;}
void RefreshLocalProfile(){const auto session=ActiveSession();if(!session)return;if(!session->premium())session->user()->setEmojiStatus(EmojiStatusId{LocalPremium()?LocalStatusDocument(session->uniqueId()):0});session->changes().peerUpdated(session->user(),Data::PeerUpdate::Flag::EmojiStatus);}
QString IconKey() {
 const auto key = Prefs().value("icon", "original").toString();
 return kIcons.contains(key) ? key : QString("original");
}
QString IconPath() { return QString(":/nyx/icons/%1.png").arg(IconKey()); }
QString ShortcutIconPath() { return Materialize(QString(":/nyx/icons/%1.ico").arg(IconKey()), QString("icon-%1.ico").arg(IconKey())); }
QStringList LocalPins(quint64 owner) { return ReadPins(owner).ids; }
int LocalPinRank(quint64 owner, quint64 peer) { return owner ? ReadPins(owner).ranks.value(peer, -1) : -1; }
bool ToggleLocalPin(quint64 owner, quint64 peer) {
 if (!owner || !peer) return false;
 auto &state = ReadPins(owner); const auto id = QString::number(peer);
 if (!state.ids.removeOne(id)) {
  if (state.ids.size() >= 500) {
   ShowNotice(QString::fromUtf8("Вы можете добавить до 500 локальных закреплений. Уберите одно из существующих."));
   return false;
  }
  state.ids.prepend(id);
 }
 state.ranks.clear();for (int i=0;i<state.ids.size();++i) state.ranks.insert(state.ids[i].toULongLong(),i);
 Prefs().setValue(QString("pins/%1").arg(owner), state.ids);Prefs().sync();
 return true;
}
QStringList IconKeys() { return kIcons; }
QStringList IconTitles() { return kTitles; }
void SetIconKey(const QString &key) {
 if (!kIcons.contains(key)) return;
 Prefs().setValue("icon", key); Prefs().sync();
 Core::App().refreshApplicationIcon();
#ifdef Q_OS_WIN
 Platform::AppUserModelId::ValidateShortcut();
#endif
}
int DeletedMarker() { return qBound(0, Prefs().value("deletedIcon", 0).toInt(), 3); }
void SetDeletedMarker(int index) {
 Prefs().setValue("deletedIcon", qBound(0, index, 3)); Prefs().sync();
 RefreshDeletedCopies(ActiveSession());
}
void SetDeletedColor(const QString &color) {
 const QStringList allowed = { "#d976a6", "#a98bfa", "#70d9e8", "#b7bed2" };
 if (!allowed.contains(color)) return;
 Prefs().setValue("deletedColor", color); Prefs().sync();
 RefreshDeletedCopies(ActiveSession());
}
bool StartupEnabled() { return Prefs().value("startup", true).toBool(); }
void SetStartupEnabled(bool enabled) { Prefs().setValue("startup", enabled); Prefs().sync(); }
bool ApplyNyxTheme() {
 const auto path = Materialize(":/nyx/night.tdesktop-theme", "nyx-night.tdesktop-theme");
 if (path.isEmpty() || !Window::Theme::Apply(path)) return false;
 Window::Theme::KeepApplied(); return true;
}
void RestoreNightTheme() {
 Window::Theme::ApplyDefaultWithPath(QString(":/gui/night.tdesktop-theme"));
 Window::Theme::KeepApplied();
}
void ShowSettings() {
 if (const auto window = Core::App().activeWindow()) {
  if (const auto controller = window->sessionController()) {
   controller->showSettings(Settings::NyxgramMain::Id());
  }
 }
}
void ShowStartup() {
 if (!Prefs().value("startup",true).toBool() || anim::Disabled() || Core::App().passcodeLocked()) return;
 auto active = QApplication::activeWindow();
 if (active && (active->isMinimized() || !active->isVisible())) return;
 auto view = new Startup;
 if (active) view->move(active->geometry().center()-QPoint(view->width()/2,view->height()/2));
 view->show();
}
}
