// Native Nyxgram settings: no independent window, stylesheet or Qt settings tabs.
#include "settings/settings_nyxgram.h"

#include "settings/settings_advanced.h"
#include "settings/settings_chat.h"
#include "nyx/nyx_features.h"
#include "nyx/nyx_settings.h"
#include "nyx/nyx_history.h"
#include "main/main_session.h"
#include "data/data_session.h"
#include "data/data_chat_filters.h"
#include "window/window_session_controller.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/widgets/buttons.h"
#include "ui/layers/generic_box.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/boxes/confirm_box.h"
#include "ui/vertical_list.h"
#include "ui/ui_utility.h"
#include "base/options.h"
#include "ui/chat/chat_style_radius.h"
#include "styles/style_settings.h"
#include "styles/style_menu_icons.h"
#include <algorithm>

namespace Settings {
namespace {
namespace N = ::Nyxgram;
enum class Page { Main, Privacy, Appearance, History, Performance, Profile, Updates };
QString Text(const char *value) { return QString::fromUtf8(value); }

// A section is bound to its controller's session, never to whichever account later
// happens to be active. This also guards delayed choice and confirmation callbacks.
bool Current(not_null<Window::SessionController*> controller) {
 return N::ActiveFeatureSession() == &controller->session();
}
void Action(not_null<Ui::VerticalLayout*> layout, const char *label, Fn<void()> done) {
 AddButtonWithIcon(layout, rpl::single(Text(label)), st::settingsButtonNoIcon)
  ->addClickHandler(std::move(done));
}
void Toggle(not_null<Ui::VerticalLayout*> layout, const char *label, bool initial,
 Fn<void(bool)> done) {
 layout->add(object_ptr<Button>(layout, rpl::single(Text(label)), st::settingsButtonNoIcon))
  ->toggleOn(rpl::single(initial))->toggledChanges()
  | rpl::start_with_next(std::move(done), layout->lifetime());
}
void Note(not_null<Ui::VerticalLayout*> layout, const char *text) {
 Ui::AddDividerText(layout, rpl::single(Text(text)));
}
void Choice(not_null<Ui::VerticalLayout*> layout,
 not_null<Window::SessionController*> controller, const char *label,
 std::vector<QString> options, Fn<int()> current, Fn<void(int)> save) {
 if (options.empty()) return;
 const auto index = std::clamp(current(), 0, int(options.size()) - 1);
 const auto value = layout->lifetime().make_state<rpl::variable<QString>>(options[index]);
 const auto button = AddButtonWithLabel(layout, rpl::single(Text(label)),
  value->value(), st::settingsButtonNoIcon);
 button->addClickHandler([=] {
  if (!Current(controller)) return;
  controller->show(Box([=](not_null<Ui::GenericBox*> box) {
   SingleChoiceBox(box, {
    .title = rpl::single(Text(label)),
    .options = options,
    .initialSelection = std::clamp(current(), 0, int(options.size()) - 1),
    .callback = crl::guard(layout, [=](int chosen) {
     if (!Current(controller) || chosen < 0 || chosen >= int(options.size())) return;
     save(chosen); *value = options[chosen];
    }),
   });
  }));
 });
}
void Setup(not_null<Ui::RpWidget*> section,
 not_null<Window::SessionController*> controller, Page page, Fn<void(Type)> navigate) {
 const auto content = Ui::CreateChild<Ui::VerticalLayout>(section);
 const auto owner = controller->session().uniqueId();
 const auto session = &controller->session();
 const auto policy = [=](const char *label, const char *key, bool fallback = false) {
  Toggle(content, label, N::Policy(owner, key, fallback), [=](bool enabled) {
   if (!Current(controller)) return;
   N::SetPolicy(owner, key, enabled); N::ApplyPerformance();
  });
 };
 const auto action = [=](const char *label, Fn<void()> done) {
  Action(content, label, [=] { if (Current(controller)) done(); });
 };
 const auto open = [=](const char *label, Type type) {
  action(label, [=] { navigate(type); });
 };
 Ui::AddSkip(content);
 switch (page) {
 case Page::Main: {
  Ui::AddSubsectionTitle(content, rpl::single(Text("Nyxgram")));
  open("Приватность", NyxgramPrivacy::Id());
  open("Оформление и иконка", NyxgramAppearance::Id());
  open("Удалённые сообщения и история", NyxgramHistory::Id());
  open("Производительность", NyxgramPerformance::Id());
  open("Профили и папки", NyxgramProfile::Id());
  open("Обновления и перенос настроек", NyxgramUpdates::Id());
  Ui::AddDivider(content); Ui::AddSkip(content);
  Toggle(content, "Призрачный режим", N::Policy(owner,"hideRead")
   && N::Policy(owner,"hideOnline") && N::Policy(owner,"hideTyping"), [=](bool on) {
    if (!Current(controller)) return;
    for (const auto key : {"hideRead","hideOnline","hideTyping"}) N::SetPolicy(owner,key,on);
   });
  Note(content, "Не отправлять прочтения, сетевой статус и «печатает» с этого устройства. Другие клиенты работают независимо.");
 } break;
 case Page::Privacy:
  Ui::AddSubsectionTitle(content, rpl::single(Text("Приватность на этом устройстве")));
  policy("Не отправлять отметки прочтения", "hideRead");
  policy("Скрывать сетевой статус", "hideOnline");
  policy("Не отправлять «печатает» и запись медиа", "hideTyping");
  policy("Не отправлять просмотр историй", "storyGhost");
  Note(content, "Экспериментально: ответы, реакции и другие клиенты могут раскрыть просмотр истории. Гарантии анонимности нет.");
  policy("Скрывать собственный номер", "hidePhone");
  policy("Подтверждать исходящие звонки", "confirmCalls");
  policy("Подтверждать отправку стикеров", "confirmStickers", true);
  policy("Подтверждать отправку GIF", "confirmGifs", true);
  break;
 case Page::Appearance: {
  Ui::AddSubsectionTitle(content, rpl::single(Text("Приложение")));
  const auto keys = N::IconKeys(); const auto titles = N::IconTitles();
  Choice(content, controller, "Иконка приложения", std::vector<QString>(titles.begin(),titles.end()),
   [=] { return keys.indexOf(N::IconKey()); }, [=](int i) { N::SetIconKey(keys[i]); });
  Toggle(content, "Показывать заставку Nyxgram", N::StartupEnabled(), N::SetStartupEnabled);
  action("Тема «Nyx ночной»", [=] {
   if (!N::ApplyNyxTheme()) controller->show(Ui::MakeInformBox(Text("Не удалось применить тему.")));
  });
  action("Стандартная ночная тема", N::RestoreNightTheme);
  open("Темы, фон чата и размер шрифта", Chat::Id());
  Ui::AddDivider(content); Ui::AddSkip(content);
  Ui::AddSubsectionTitle(content, rpl::single(Text("Переписка")));
  policy("Скрывать время стикеров", "stickerTime");
  policy("Скрывать время GIF", "gifTime");
  policy("Скрывать время видеосообщений", "roundTime");
  policy("Скрывать аватары в переписке", "avatars");
  policy("Скрывать разделители дат", "dates");
  Toggle(content, "Компактная форма пузырей", base::options::lookup<bool>(Ui::kOptionUseSmallMsgBubbleRadius).value(),
   [=](bool on) { if (Current(controller)) base::options::lookup<bool>(Ui::kOptionUseSmallMsgBubbleRadius).set(on); });
  Note(content, "Для полного обновления геометрии переписки перезапустите клиент.");
  Toggle(content, "Локальный Premium", N::LocalPremium(owner), [=](bool on) {
   if (!Current(controller)) return;
   N::SetPolicy(owner,"localPremium",on); N::RefreshLocalProfile();
  });
  Note(content, "Только локальное оформление и эмодзи-статус. Серверная подписка, лимиты и платные функции не меняются.");
 } break;
 case Page::History: {
  Ui::AddSubsectionTitle(content, rpl::single(Text("Удалённые сообщения")));
  policy("Оставлять удалённые сообщения в чатах", "keepDeleted", N::KeepDeletedMessages(owner));
  policy("Сохранять историю редактирования", "keepEdits");
  Note(content, "Полученные на этом устройстве сообщения остаются приглушёнными, со значком удаления. Локальные копии шифруются Windows. Сообщения с таймером и одноразовые медиа не сохраняются.");
  Choice(content, controller, "Значок удаления", {Text("Корзина"),Text("Удаление"),Text("Точка"),Text("Контур")},
   N::DeletedMarker, N::SetDeletedMarker);
  const auto colors = QStringList{"#d976a6","#a98bfa","#70d9e8","#b7bed2"};
  Choice(content, controller, "Цвет значка", {Text("Розовый"),Text("Лавандовый"),Text("Голубой"),Text("Серый")},
   [=] { return colors.indexOf(N::DeletedColor().name()); }, [=](int i) { N::SetDeletedColor(colors[i]); });
  const auto days = std::vector<int>{7,30,90,365,0};
  Choice(content, controller, "Хранить локальную историю", {Text("7 дней"),Text("30 дней"),Text("90 дней"),Text("365 дней"),Text("Без срока")},
   [=] { const auto i = std::find(days.begin(),days.end(),N::NumberPolicy(owner,"retention",30)); return int(i-days.begin()); },
   [=](int i) { N::SetNumberPolicy(owner,"retention",days[i]); });
  Ui::AddDivider(content); Ui::AddSkip(content);
  action("История и поиск", [=] { N::ShowLocalHistory(session); });
  action("Установить / сменить PIN истории", [=] { N::ChangeArchivePin(section,session); });
  action("Заблокировать локальную историю", N::LockArchive);
  action("Отключить PIN истории", [=] { N::DisableArchivePin(section,session); });
  action("Очистить локальную историю", [=] {
   N::RequestArchiveAccess(session, crl::guard(content,[=] {
    if (!Current(controller)) return;
   controller->show(Ui::MakeConfirmBox({
    .text = rpl::single(Text("Удалить локальные копии и версии? Сообщения на сервере не изменятся.")),
    .confirmed = crl::guard(content, [=] {
     if (Current(controller) && N::ArchiveAllowed(owner)) N::ClearDeletedCopies(session);
    }),
    .confirmText = rpl::single(Text("Удалить")),
   }));
   }));
  });
 } break;
 case Page::Performance:
  Ui::AddSubsectionTitle(content, rpl::single(Text("Экономия ресурсов")));
  policy("Отключить анимации и эффекты", "noAnimations");
  policy("Отключить анимацию эмодзи", "noEmoji");
  policy("Отключить анимацию стикеров", "noStickers");
  policy("Не проигрывать GIF автоматически", "noGifs");
  policy("Не проигрывать видео автоматически", "noVideos");
  policy("Не загружать медиа автоматически", "noDownloads");
  policy("Отключить размытие", "noBlur");
  Note(content, "Явно выбранное медиа можно просмотреть в предпросмотре независимо от автопроигрывания в чатах.");
  open("Загрузка медиа и память устройства", Advanced::Id());
  break;
 case Page::Profile: {
  Ui::AddSubsectionTitle(content, rpl::single(Text("Профили")));
  policy("Показывать ID в профиле", "showId", true);
  policy("Показывать DC в профиле", "showDc", true);
  action("Сохранённые профили", [=] { N::ShowSavedProfiles(section,session); });
  std::vector<QString> labels{Text("Все чаты")}; std::vector<int> ids{0};
  for (const auto &f : session->data().chatsFilters().list()) if (f.id()>0) {
   labels.push_back(f.titleText().text); ids.push_back(f.id());
  }
  Choice(content, controller, "Папка при запуске", labels,
   [=] { const auto i=std::find(ids.begin(),ids.end(),N::NumberPolicy(owner,"startupFolder",0)); return i==ids.end()?0:int(i-ids.begin()); },
   [=](int i) { N::SetNumberPolicy(owner,"startupFolder",ids[i]); });
  Note(content, "Локальные закрепления доступны в меню чата. Они не синхронизируются и не меняют серверные папки.");
 } break;
 case Page::Updates:
  Ui::AddSubsectionTitle(content, rpl::single(Text("Обновления Nyxgram")));
  policy("Проверять обновления", "autoUpdates", true);
  action("Проверить обновления Windows", [] { N::CheckDesktopUpdates(true); });
  Ui::AddDivider(content); Ui::AddSkip(content);
  action("Экспортировать настройки", [=] { N::TransferSettings(section,session,false); });
  action("Импортировать настройки", [=] { N::TransferSettings(section,session,true); if (Current(controller)) navigate(NyxgramMain::Id()); });
  Note(content, "Переносятся параметры интерфейса и приватности. Сообщения, PIN и данные входа не экспортируются.");
  action("Сбросить настройки возможностей", [=] {
   controller->show(Ui::MakeConfirmBox({
    .text = rpl::single(Text("Сбросить параметры возможностей? Аккаунт, локальная история и PIN останутся.")),
    .confirmed = crl::guard(content, [=] { if (Current(controller)) { N::ResetFeatureSettings(owner); navigate(NyxgramMain::Id()); } }),
    .confirmText = rpl::single(Text("Сбросить")),
   }));
  });
  break;
 }
 Ui::AddSkip(content);
 Ui::ResizeFitChild(section,content);
}
} // namespace

NyxgramMain::NyxgramMain(QWidget *parent, not_null<Window::SessionController*> controller)
: Section(parent) { Setup(this, controller, Page::Main, showOtherMethod()); }
rpl::producer<QString> NyxgramMain::title() { return rpl::single(Text("Nyxgram")); }

NyxgramPrivacy::NyxgramPrivacy(QWidget *parent, not_null<Window::SessionController*> controller)
: Section(parent) { Setup(this, controller, Page::Privacy, showOtherMethod()); }
rpl::producer<QString> NyxgramPrivacy::title() { return rpl::single(Text("Приватность")); }

NyxgramAppearance::NyxgramAppearance(QWidget *parent, not_null<Window::SessionController*> controller)
: Section(parent) { Setup(this, controller, Page::Appearance, showOtherMethod()); }
rpl::producer<QString> NyxgramAppearance::title() { return rpl::single(Text("Оформление Nyxgram")); }

NyxgramHistory::NyxgramHistory(QWidget *parent, not_null<Window::SessionController*> controller)
: Section(parent) { Setup(this, controller, Page::History, showOtherMethod()); }
rpl::producer<QString> NyxgramHistory::title() { return rpl::single(Text("Удалённые сообщения и история")); }

NyxgramPerformance::NyxgramPerformance(QWidget *parent, not_null<Window::SessionController*> controller)
: Section(parent) { Setup(this, controller, Page::Performance, showOtherMethod()); }
rpl::producer<QString> NyxgramPerformance::title() { return rpl::single(Text("Производительность")); }

NyxgramProfile::NyxgramProfile(QWidget *parent, not_null<Window::SessionController*> controller)
: Section(parent) { Setup(this, controller, Page::Profile, showOtherMethod()); }
rpl::producer<QString> NyxgramProfile::title() { return rpl::single(Text("Профили и папки")); }

NyxgramUpdates::NyxgramUpdates(QWidget *parent, not_null<Window::SessionController*> controller)
: Section(parent) { Setup(this, controller, Page::Updates, showOtherMethod()); }
rpl::producer<QString> NyxgramUpdates::title() { return rpl::single(Text("Обновления Nyxgram")); }

} // namespace Settings
