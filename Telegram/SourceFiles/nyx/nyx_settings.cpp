// Nyxgram-only visual preferences and local ordering. No subscription or MTProto changes.
#include "nyx/nyx_settings.h"
#include "nyx/nyx_history.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "main/main_session.h"
#include "data/data_user.h"
#include "data/data_emoji_statuses.h"
#include "data/data_session.h"
#include "data/data_changes.h"
#include <QtCore/QDateTime>
#include <QtWidgets/QComboBox>
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
#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
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
bool LocalPremium(){return Prefs().value("localPremium",false).toBool();}
bool KeepDeletedMessages(){return Prefs().value("keepDeleted",false).toBool();}
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
   QMessageBox::information(QApplication::activeWindow(),"Nyxgram",QString::fromUtf8("Вы можете добавить до 500 локальных закреплений. Уберите одно из существующих."));
   return false;
  }
  state.ids.prepend(id);
 }
 state.ranks.clear();for (int i=0;i<state.ids.size();++i) state.ranks.insert(state.ids[i].toULongLong(),i);
 Prefs().setValue(QString("pins/%1").arg(owner), state.ids);Prefs().sync();
 return true;
}
void ShowSettings() {
 static QPointer<QDialog> open;
 if (open) { open->raise();open->activateWindow();return; }
 auto dialog = new QDialog(QApplication::activeWindow());open=dialog;
 dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle("Nyxgram · 0.2.1");dialog->setWindowIcon(QIcon(IconPath()));dialog->resize(600,690);
 dialog->setStyleSheet("QDialog, QWidget#nyxBody {background:#0b1020;color:#f1f3fa;} QLabel,QCheckBox {color:#f1f3fa;} QToolButton,QPushButton {background:#141b2d;color:#f1f3fa;border:1px solid #36445e;border-radius:8px;padding:8px;} QToolButton:checked {border:2px solid #a98bfa;background:#26324a;} QToolButton:hover,QPushButton:hover {background:#26324a;} QCheckBox {spacing:10px;padding:10px 0;} QCheckBox::indicator {width:22px;height:22px;} QScrollArea {border:0;}");
 auto root = new QVBoxLayout(dialog); root->setContentsMargins(24,24,24,24);root->setSpacing(16);
 auto area = new QScrollArea(dialog);area->setWidgetResizable(true);root->addWidget(area);
 auto body = new QWidget;body->setObjectName("nyxBody");area->setWidget(body);
 auto layout = new QVBoxLayout(body);layout->setSpacing(16);
 auto label=[&](const char *text) {auto l=new QLabel(QString::fromUtf8(text));l->setWordWrap(true);layout->addWidget(l);return l;};
 auto heading=label("Ваш Nyxgram");heading->setStyleSheet("font-size:26px;font-weight:600;");
 label("16 собственных иконок. Все доступны без Telegram Premium.");
 auto grid=new QGridLayout;grid->setSpacing(12);layout->addLayout(grid);
 auto buttons=std::make_shared<QList<QToolButton*>>();
 for(int i=0;i<kIcons.size();++i) {
  auto b=new QToolButton(body);buttons->append(b);b->setText(kTitles[i]);b->setIcon(QIcon(QString(":/nyx/icons/%1.png").arg(kIcons[i])));b->setIconSize(QSize(62,62));b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);b->setCheckable(true);b->setChecked(kIcons[i]==IconKey());b->setMinimumSize(96,100);b->setAccessibleName(kTitles[i]);grid->addWidget(b,i/4,i%4);
  QObject::connect(b,&QToolButton::clicked,dialog,[=] {
   Prefs().setValue("icon",kIcons[i]);Prefs().sync();
   for(int j=0;j<buttons->size();++j) (*buttons)[j]->setChecked(j==i);
   dialog->setWindowIcon(QIcon(IconPath()));Core::App().refreshApplicationIcon();
#ifdef Q_OS_WIN
   Platform::AppUserModelId::ValidateShortcut();
#endif
  });
 }
 auto premium=new QCheckBox(QString::fromUtf8("Локальный Telegram Premium"));premium->setChecked(Prefs().value("localPremium",false).toBool());layout->addWidget(premium);
 auto note=label(premium->isChecked()?"Локальная звезда Premium, выбор эмодзи и локальный эмодзи-статус включены. Серверная подписка не меняется.":"Звезда Premium в собственном профиле, выбор анимированных эмодзи и локальный эмодзи-статус. Другие пользователи не видят локальный статус. Серверные лимиты, бусты, расшифровка и платные реакции не меняются.");
 QObject::connect(premium,&QCheckBox::toggled,dialog,[=](bool checked){Prefs().setValue("localPremium",checked);Prefs().sync();RefreshLocalProfile();note->setText(QString::fromUtf8(checked?"Локальная звезда Premium, выбор эмодзи и локальный эмодзи-статус включены. Серверная подписка не меняется.":"Локальный Premium выключен. Бесплатные иконки остаются доступны."));});
 auto deleted=new QCheckBox(QString::fromUtf8("Показывать удалённые сообщения в чатах"));deleted->setChecked(KeepDeletedMessages());layout->addWidget(deleted);
 QObject::connect(deleted,&QCheckBox::toggled,dialog,[](bool checked){Prefs().setValue("keepDeleted",checked);Prefs().sync();if(!checked&&Core::App().domain().started()&&Core::App().domain().active().sessionExists())HideDeletedCopies(&Core::App().domain().active().session());});
 label("После удаления сообщение остаётся на месте: немного приглушённое, со значком рядом со временем. Копии шифруются для текущего пользователя Windows. Секретные, одноразовые сообщения и сообщения с таймером не сохраняются. Только полученные на этом устройстве сообщения; до 5000 копий / 8 МБ метаданных на аккаунт.");
 auto clearCopies=new QPushButton(QString::fromUtf8("Очистить локальные копии удалённых сообщений"));layout->addWidget(clearCopies);
 QObject::connect(clearCopies,&QPushButton::clicked,dialog,[dialog]{if(QMessageBox::question(dialog,"Nyxgram",QString::fromUtf8("Удалить все сохранённые копии текущего аккаунта на этом компьютере? Сообщения на сервере не изменятся."))==QMessageBox::Yes)ClearDeletedCopies(ActiveSession());});
 auto marks=new QComboBox(body);marks->addItems({QString::fromUtf8("✕ Крестик"),QString::fromUtf8("⌫ Удаление"),QString::fromUtf8("● Точка"),QString::fromUtf8("▱ Контур")});marks->setCurrentIndex(Prefs().value("deletedIcon",0).toInt());layout->addWidget(marks);
 QObject::connect(marks,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,[](int index){Prefs().setValue("deletedIcon",index);RefreshDeletedCopies(ActiveSession());});
 auto colors=new QComboBox(body);colors->addItems({QString::fromUtf8("Розовый"),QString::fromUtf8("Лавандовый"),QString::fromUtf8("Голубой"),QString::fromUtf8("Серый")});const QStringList colorValues={"#d976a6","#a98bfa","#70d9e8","#b7bed2"};colors->setCurrentIndex(qMax(0,colorValues.indexOf(Prefs().value("deletedColor","#d976a6").toString())));layout->addWidget(colors);
 QObject::connect(colors,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,[colorValues](int index){Prefs().setValue("deletedColor",colorValues[index]);RefreshDeletedCopies(ActiveSession());});
 auto startup=new QCheckBox(QString::fromUtf8("Показывать заставку Nyxgram"));startup->setChecked(Prefs().value("startup",true).toBool());layout->addWidget(startup);
 QObject::connect(startup,&QCheckBox::toggled,dialog,[](bool checked){Prefs().setValue("startup",checked);});
 auto night=new QPushButton(QString::fromUtf8("Применить «Nyx ночной»"));layout->addWidget(night);
 QObject::connect(night,&QPushButton::clicked,dialog,[=]{auto path=Materialize(":/nyx/night.tdesktop-theme","nyx-night.tdesktop-theme");if(!path.isEmpty()&&Window::Theme::Apply(path))Window::Theme::KeepApplied();else QMessageBox::warning(dialog,"Nyxgram",QString::fromUtf8("Не удалось применить тему."));});
 auto restore=new QPushButton(QString::fromUtf8("Стандартная ночная тема"));layout->addWidget(restore);
 QObject::connect(restore,&QPushButton::clicked,dialog,[]{Window::Theme::ApplyDefaultWithPath(QString(":/gui/night.tdesktop-theme"));Window::Theme::KeepApplied();});
 label("Локальные закрепления: до 500 на аккаунт. Нажмите правой кнопкой на чат → «Закрепить локально · Nyxgram». Они идут после серверных закреплений, не синхронизируются и не меняют состав папок. Убрать закрепление можно в том же меню.");
 label("Иконка окна меняется сразу. Для ярлыка Windows может потребоваться обновление рабочего стола. Значок самого EXE остаётся исходным Nyxgram; дополнительные ICO включены в архив.");
 auto close=new QPushButton(QString::fromUtf8("Готово"));root->addWidget(close);QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::close);
 dialog->show();
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
