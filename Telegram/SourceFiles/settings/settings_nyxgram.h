// Nyxgram settings hosted by Telegram's own navigation and theme system.
#pragma once
#include "settings/settings_common_session.h"
namespace Settings {
class NyxgramMain final : public Section<NyxgramMain> {
public:
 NyxgramMain(QWidget *parent, not_null<Window::SessionController*> controller);
 rpl::producer<QString> title() override;
};
class NyxgramPrivacy final : public Section<NyxgramPrivacy> {
public:
 NyxgramPrivacy(QWidget *parent, not_null<Window::SessionController*> controller);
 rpl::producer<QString> title() override;
};
class NyxgramAppearance final : public Section<NyxgramAppearance> {
public:
 NyxgramAppearance(QWidget *parent, not_null<Window::SessionController*> controller);
 rpl::producer<QString> title() override;
};
class NyxgramHistory final : public Section<NyxgramHistory> {
public:
 NyxgramHistory(QWidget *parent, not_null<Window::SessionController*> controller);
 rpl::producer<QString> title() override;
};
class NyxgramPerformance final : public Section<NyxgramPerformance> {
public:
 NyxgramPerformance(QWidget *parent, not_null<Window::SessionController*> controller);
 rpl::producer<QString> title() override;
};
class NyxgramProfile final : public Section<NyxgramProfile> {
public:
 NyxgramProfile(QWidget *parent, not_null<Window::SessionController*> controller);
 rpl::producer<QString> title() override;
};
class NyxgramUpdates final : public Section<NyxgramUpdates> {
public:
 NyxgramUpdates(QWidget *parent, not_null<Window::SessionController*> controller);
 rpl::producer<QString> title() override;
};
}
