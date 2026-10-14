"""Source integration regression checks, not an end-to-end/runtime test."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]/'Telegram/SourceFiles'
def read(p): return (r/p).read_text(encoding='utf-8')
checks=0
def verify(cond,msg):
 global checks
 if not cond: raise AssertionError(msg)
 checks+=1
for file in ['history/history_widget.cpp','history/view/history_view_chat_section.cpp','history/view/history_view_scheduled_section.cpp','media/stories/media_stories_reply.cpp','settings/business/settings_shortcut_messages.cpp']:
 s=read(file);verify(s.count('Nyxgram::ConfirmMedia(')>=2,file+' needs document and inline preflight')
for file,key in [('data/data_histories.cpp','hideRead'),('api/api_updates.cpp','hideOnline'),('api/api_send_progress.cpp','hideTyping'),('data/data_stories.cpp','storyGhost')]:
 verify(key in read(file),file+' policy hook')
s=read('nyx/nyx_history.cpp')
for key in ['CryptProtectData','CryptUnprotectData','ttlDestroyAt','ventities','keepEdits','Prune','RememberLocalEmoji','RestoreLocalEmoji','Sha256']:
 verify(key in s,key)
s=read('history/history_item.cpp');a=s.index('const auto oldId = std::exchange(id, newId);');b=s.index('Nyxgram::RememberLocalEmoji(this);',a)
verify('MessageFlag::Local' in s[a:b],'clear local flag before remembering server ID')
verify(s.count('Nyxgram::RestoreLocalEmoji(this);')==2,'restore after both sent response variants')
s=read('nyx/nyx_features.cpp')
for key in ['deriveKeyPbkdf2','210000','pinRetry','HideLockedSessions','featureSessions','startupFolder','QDialogButtonBox::RejectRole']:
 verify(key in s,key)
verify('!session->premium()' in read('api/api_text_entities.cpp'),'real server entitlement required')
verify('NYXGRAM_BUILD_NUMBER' in read('nyx/nyx_updates.cpp'),'fork update build comparison')
print(f'Nyxgram Windows source integration: {checks} checks PASS (not runtime tests)')
