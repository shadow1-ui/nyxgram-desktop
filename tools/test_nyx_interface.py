"""C++ syntax/integration checks only; not a compiler or Windows UI test."""
from pathlib import Path
from tree_sitter import Language, Parser
import tree_sitter_cpp
r=Path(__file__).resolve().parents[1]/'Telegram/SourceFiles'
def read(p):return (r/p).read_text()
checks=0
def check(value,why):
 global checks
 assert value,why
 checks+=1
s=read('history/view/history_view_bottom_info.cpp')
check('marked.append(Nyxgram::DeletedSymbol()' not in s,'no duplicate timestamp glyph')
check('p.drawText(QRect(right,position.y()' not in s,'no overpainted timestamp glyph')
check('right -= DeletedMarkerWidth();' in s,'draw reserves marker space')
check('width += DeletedMarkerWidth();' in s,'layout reserves marker space')
check('PaintDeletedMarker(p, QPoint(right, position.y()), outerWidth)' in s,'single marker draw')
check('style::rtlrect(position.x()' in s,'RTL marker geometry uses the free style helper, not the RpWidget member')
check('p.save();' in s and 'p.restore();' in s,'restore icon painter state')
check('p.opacity() * .78' in read('history/view/history_view_message.cpp'),'faded local copy')
s=read('media/player/media_player_widget.cpp')
check('void Widget::keyPressEvent' in s and 'Qt::Key_Return' in s,'keyboard activation')
check('Qt::ControlModifier' in s and '_showItemCallback(item)' in s,'source message navigation preserved')
check('setFocusPolicy(Qt::StrongFocus)' in s,'keyboard focus')
check('setAccessibleName(' in s,'accessible player label')
s=read('media/player/media_player_panel.cpp')
check('instance()->current(_type)' in s,'queue uses active audio type')
check('Info::Section::MediaType::RoundVoiceFile' in s,'native voice/round-video list')
check('document->isVoiceMessage() || document->isVideoMessage()' in s,'voice media validation')
empty=s[s.index('int Panel::emptyInnerHeight()'):s.index('bool Panel::preventAutoHide()')]
check('songThumbSize' not in empty,'one-entry queue is not considered empty')
check('void Panel::setType' in s and '_a_appearance.stop()' in s,'type switch clears stale animation')
check('if (&document->session() != &session())' in s,'different-account list denied')
check('_refreshListLifetime.destroy();' in s,'queue subscription cleanup')
check('isMedia && section.mediaType()' in read('info/media/info_media_list_widget.cpp'),'saved music section precondition respected')
s=read('mainwidget.cpp')
check(s.index('setType(_player->entity()->playlistType())')<s.index('_playerPlaylist->showFromOther();'),'select type before opening')
s=read('nyx/nyx_features.cpp')
check('QTabWidget' not in s and 'void ShowFeatures(QWidget *parent) { Q_UNUSED(parent); ShowSettings(); }' in s,'legacy feature entry routes to native settings')
native=read('settings/settings_nyxgram.cpp')
header=read('settings/settings_nyxgram.h')
for name in ['Main','Privacy','Appearance','History','Performance','Profile','Updates']:
 check('class Nyxgram'+name+' final : public Section<Nyxgram'+name+'>' in header,'native section factory: '+name)
check('showOther(NyxgramMain::Id())' in read('settings/settings_main.cpp'),'native main-settings back-stack navigation')
check('controller->showSettings(Settings::NyxgramMain::Id())' in read('nyx/nyx_settings.cpp'),'legacy main entry also routes natively')
check('QDialog' not in read('nyx/nyx_settings.cpp'),'no separate Nyxgram settings window')
check('setStyleSheet' not in native and 'settingsButtonNoIcon' in native,'native theme and settings controls')
check('Current(controller)' in native and 'crl::guard(layout' in native,'session/lifetime guards for delayed choice callbacks')
check('toggledChanges()' in native,'constructing controls does not mutate preferences')
check('QInputDialog' not in s and 'void RequestArchiveAccess(' in s,'native asynchronous PIN access')
check('salt.size()!=24' in s and 'now+60' in s and '5*60*1000' in s,'PIN salt validation, retry throttle and unlock timeout')
check('QDialog' not in read('nyx/nyx_history.cpp') and 'not_null<Ui::GenericBox*>' in read('nyx/nyx_history.cpp'),'history search stays in Telegram')
archive=read('nyx/nyx_history.cpp')
check(archive.index('if(!Eligible(message))') < archive.index('if (UnsupportedSnapshot(message))'),'TTL purge precedes unsupported fallback')
check('!UnsupportedSnapshot(previous)' in archive,'keep recoverable snapshot before unsupported stub')
check('item->applyEdition(std::move(edition))' in archive,'restore captured content into retained live item')
check('if (localDeleted)' in read('history/history_item.cpp'),'local fallback does not suppress unsupported notices for ordinary messages')
check('Нажатие строки ID/DC копирует только ID.' not in s,'verbose technical text removed')
print(f'PASS: {checks} desktop interface source-integration checks (not runtime tests)')
parser=Parser(Language(tree_sitter_cpp.language()))
files=['history/view/history_view_bottom_info.cpp','history/view/history_view_message.cpp',
 'info/media/info_media_list_widget.cpp','mainwidget.cpp','media/player/media_player_panel.cpp',
 'media/player/media_player_panel.h','media/player/media_player_widget.cpp','media/player/media_player_widget.h',
 'nyx/nyx_features.cpp','nyx/nyx_settings.cpp','nyx/nyx_settings.h',
 'settings/settings_nyxgram.cpp','settings/settings_nyxgram.h','nyx/nyx_history.cpp','history/history_item.cpp']
for f in files:
 tree=parser.parse((r/f).read_bytes())
 check(not tree.root_node.has_error,'C++ parser syntax: '+f)
# info_profile_actions.cpp contains an existing preprocessor/parser ambiguity: the changed
# label is checked separately, not misreported as a fresh successful full-file parse.
check('rpl::single(QString::fromUtf8("Информация"))' in read('info/profile/info_profile_actions.cpp'),'compact profile heading')
print(f'PASS: {len(files)} changed C++ units parsed (no type checking); profile label checked separately')
# Check the palette source and Qt resource alias; not a replacement for native theme loading.
import re, xml.etree.ElementTree as ET
resources=r.parent/'Resources'
palette=(resources/'nyx/night-refined.tdesktop-palette').read_text()
entries=re.findall(r'(?m)^(\w+):\s*([^;]+);',palette)
colors=dict(entries)
check(len(entries)==len(colors),'no duplicate palette roles')
for key,value in entries:
 check(bool(re.fullmatch(r'#[0-9a-fA-F]{6}(?:[0-9a-fA-F]{2})?',value)) or value in colors,'palette value: '+key)
def resolve(key):
 value=colors[key]
 return value if value.startswith('#') else resolve(value)
def luminance(color):
 rgb=[int(color[n:n+2],16)/255 for n in (1,3,5)]
 rgb=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb]
 return sum(a*b for a,b in zip(rgb,(.2126,.7152,.0722)))
for foreground,background in [('activeButtonFg','activeButtonBg'),('activeButtonFgOver','activeButtonBgOver'),('historyTextInFg','msgInBg'),('historyTextOutFg','msgOutBg'),('msgInDateFg','msgInBg'),('msgOutDateFg','msgOutBg')]:
 a,b=sorted((luminance(resolve(foreground)),luminance(resolve(background))))
 check((b+.05)/(a+.05)>=4.5,'text contrast: '+foreground)
qrc=ET.parse(resources/'qrc/telegram/telegram.qrc')
check(any(el.text=='../../nyx/night-refined.tdesktop-palette' and el.get('alias')=='night.tdesktop-theme' for el in qrc.iter('file')),'refined theme packaged')
print('PASS: palette references, Qt alias and six 4.5:1 text-contrast pairs (no rendered UI QA)')

# Native Windows build regressions: Qt 5.15 and the pinned lib_ui callback API.
history=read('nyx/nyx_history.cpp')
check('state.edits.removeIf' not in history,'QList::removeIf requires Qt 6')
check('state.edits.erase(std::remove_if(' in history,'Qt 5 compatible TTL purge')
check('#include "data/data_document.h"' in read('history/view/history_view_message.cpp'),'complete DocumentData for media member calls')
profile=read('info/profile/info_profile_actions.cpp')
check('overrideLinkClickHandler([peer=_peer](const ClickHandlerPtr' not in profile,'link override is not a click-filter callback')
check(profile.count('overrideLinkClickHandler([peer=_peer]{')==3,'three native no-argument link overrides')
print('PASS: five Qt 5/lib_ui Windows compiler regression guards')

# Ensure every newly changed rendering/player integration unit gets a native preflight.
cmake=(r.parent/'CMakeLists.txt').read_text()
preflight=cmake[cmake.index('add_library(nyx_compile_check OBJECT'):cmake.index('target_link_libraries(nyx_compile_check')]
for unit in ['settings/settings_nyxgram.cpp','history/view/history_view_bottom_info.cpp','media/player/media_player_widget.cpp','media/player/media_player_panel.cpp','info/media/info_media_list_widget.cpp','mainwidget.cpp']:
 check('${src_loc}/'+unit in preflight,'native compile preflight: '+unit)
print('PASS: all six native settings/rendering/player units included in native C++ preflight')

# Manual stable publication is explicit and remains downstream of successful compilation.
workflow=(r.parents[1]/'.github/workflows/win.yml').read_text()
check('publish_release:' in workflow and 'PUBLISH_RELEASE: ${{ inputs.publish_release }}' in workflow,'explicit release input')
check("$extra = @('--draft')" in workflow,'draft remains default')
check(workflow.index('Compile Nyxgram Desktop Release') < workflow.index('Verify Windows update package before release') < workflow.index('Publish requested release or prepare draft'),'verify package after native build and before publication')
check("$env:PUBLISH_RELEASE -eq 'true'" in workflow and "@('--latest')" in workflow,'stable publication requires explicit opt-in')
print('PASS: four release publication guards (native build, manifest and explicit opt-in)')
