// Owner-scoped local copies. Native deletion updates and PTS processing are not suppressed.
#include "nyx/nyx_history.h"
#include "nyx/nyx_features.h"
#include <QtCore/QDateTime>
#include <QtCore/QCryptographicHash>
#include "data/data_user.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTextEdit>
#include "nyx/nyx_settings.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include <QtCore/QSettings>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>
#include <QtCore/QDir>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>
#include <cstring>
#include <algorithm>
#include <QtCore/QSet>
#include "core/utils.h"
#include "settings.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif
namespace Nyxgram {
namespace {
constexpr int kLimit = 5000;
constexpr int kMaxBytes = 8*1024*1024;
struct Version {QString key;QByteArray body;qint64 at;};
struct Archive { QHash<QString,QByteArray> snapshots, deleted; QStringList order; QList<Version> edits;QHash<QString,qint64> deletedAt;bool dirty=false; };
QHash<quint64,Archive> archives;
QString Key(quint64 peer,int mid){return QString::number(peer)+":"+QString::number(mid);}
QString Path(quint64 owner){return cWorkingDir()+QString("tdata/nyx/deleted-%1.bin").arg(owner);}
QByteArray Protect(const QByteArray &bytes, quint64 owner, bool encrypt){
#ifdef Q_OS_WIN
 const auto entropy=QString::number(owner).toUtf8();
 DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(const_cast<char*>(bytes.constData()))};
 DATA_BLOB extra{DWORD(entropy.size()),reinterpret_cast<BYTE*>(const_cast<char*>(entropy.constData()))},out{};
 const auto ok=encrypt?CryptProtectData(&input,L"Nyxgram local messages",&extra,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out):CryptUnprotectData(&input,nullptr,&extra,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out);
 if(!ok)return {};
 QByteArray result(reinterpret_cast<const char*>(out.pbData),int(out.cbData));LocalFree(out.pbData);return result;
#else
 Q_UNUSED(bytes);Q_UNUSED(owner);Q_UNUSED(encrypt);return {};
#endif
}
bool Eligible(const MTPMessage &message){
 if(message.type()!=mtpc_message)return false;
 const auto &m=message.c_message();
 if(m.vid().v<=0 || m.vttl_period().value_or_empty()>0)return false;
 if(const auto media=m.vmedia()){
  if(media->type()==mtpc_messageMediaPhoto&&media->c_messageMediaPhoto().vttl_seconds().value_or_empty()>0)return false;
  if(media->type()==mtpc_messageMediaDocument&&media->c_messageMediaDocument().vttl_seconds().value_or_empty()>0)return false;
 }
 return true;
}
QByteArray Encode(const MTPMessage &message){mtpBuffer buffer;message.write(buffer);return QByteArray(reinterpret_cast<const char*>(buffer.data()),int(buffer.size()*sizeof(mtpPrime)));}
bool Decode(const QByteArray &raw,MTPMessage &result){
 if(raw.isEmpty() || raw.size()>512*1024 || raw.size()%sizeof(mtpPrime))return false;
 mtpBuffer buffer;buffer.resize(raw.size()/sizeof(mtpPrime));std::memcpy(buffer.data(),raw.constData(),raw.size());
 const auto *from=buffer.data(),*end=from+buffer.size();
 return result.read(from,end)&&from==end&&Eligible(result);
}
Archive &Read(quint64 owner){
 if(!archives.contains(owner)){
  Archive state;QFile file(Path(owner));
  if(file.size()<=kMaxBytes+1024*1024 && file.open(QIODevice::ReadOnly)){
   const auto clear=Protect(file.readAll(),owner,false);const auto array=QJsonDocument::fromJson(clear).array();
   for(const auto &value:array){auto o=value.toObject();const auto peer=o.value("peer").toString().toULongLong();const auto mid=o.value("mid").toInt();const auto body=QByteArray::fromBase64(o.value("body").toString().toLatin1());MTPMessage m;
    if(peer&&mid>0&&Decode(body,m)&&PeerFromMessage(m).value==peer&&IdFromMessage(m).bare==mid){auto key=Key(peer,mid);const auto at=o.value("at").toVariant().toLongLong();if(o.value("kind").toString()=="edit"){if(state.edits.size()<3000)state.edits.append({key,body,at});}else if(state.deleted.size()<kLimit){state.deleted.insert(key,body);state.deletedAt.insert(key,at);state.order.append(key);}}
   }
  }
  archives.insert(owner,state);
 }
 return archives[owner];
}
void Prune(quint64 owner,Archive &state){const auto days=NumberPolicy(owner,"retention",30);if(!days)return;const auto cutoff=QDateTime::currentSecsSinceEpoch()-qint64(days)*86400;for(auto i=state.order.begin();i!=state.order.end();){const auto at=state.deletedAt.value(*i);if(at&&at<cutoff){state.deleted.remove(*i);state.deletedAt.remove(*i);i=state.order.erase(i);state.dirty=true;}else ++i;}for(auto i=state.edits.begin();i!=state.edits.end();)if(i->at&&i->at<cutoff){i=state.edits.erase(i);state.dirty=true;}else ++i;}
void ScheduleFlush(){
 static QTimer *timer=[] {auto t=new QTimer(QCoreApplication::instance());t->setSingleShot(true);QObject::connect(t,&QTimer::timeout,[]{FlushDeletedCopies();});QObject::connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,[]{FlushDeletedCopies();});return t;}();
 timer->start(200);
}
}
void HideDeletedCopies(Main::Session *session){
 if(!session)return;const auto keys=Read(session->uniqueId()).order;
 for(const auto &key:keys){const auto parts=key.split(':');if(const auto item=session->data().message(PeerId(parts[0].toULongLong()),MsgId(parts[1].toLongLong()));item&&item->nyxDeleted)item->destroy();}
}
void ClearDeletedCopies(Main::Session *session){
 if(!session)return;HideDeletedCopies(session);const auto owner=session->uniqueId();auto &state=Read(owner);state.deleted.clear();state.deletedAt.clear();state.edits.clear();state.order.clear();state.dirty=false;QFile::remove(Path(owner));
}
void RefreshDeletedCopies(Main::Session *session){
 if(!session)return;const auto keys=Read(session->uniqueId()).order;for(const auto &key:keys){const auto parts=key.split(':');if(const auto item=session->data().message(PeerId(parts[0].toULongLong()),MsgId(parts[1].toLongLong()));item&&item->nyxDeleted){session->data().notifyItemDataChange(item);session->data().requestItemResize(item);session->data().requestItemRepaint(item);}}
}
void FlushDeletedCopies(){
 QDir().mkpath(cWorkingDir()+QString("tdata/nyx/"));
 for(auto i=archives.begin();i!=archives.end();++i){auto &state=i.value();Prune(i.key(),state);if(!state.dirty)continue;QJsonArray records;
  for(const auto &key:state.order){const auto body=state.deleted.value(key);if(body.isEmpty())continue;const auto parts=key.split(':');QJsonObject o;o["peer"]=parts[0];o["mid"]=parts[1].toInt();o["body"]=QString::fromLatin1(body.toBase64());o["at"]=state.deletedAt.value(key);records.append(o);}
  for(const auto &v:state.edits){const auto parts=v.key.split(':');QJsonObject o;o["kind"]="edit";o["peer"]=parts[0];o["mid"]=parts[1].toInt();o["body"]=QString::fromLatin1(v.body.toBase64());o["at"]=v.at;records.append(o);}
  auto clear=QJsonDocument(records).toJson(QJsonDocument::Compact);
  while(clear.size()>kMaxBytes&&!records.isEmpty()){const auto first=records.takeAt(0).toObject();const auto key=Key(first["peer"].toString().toULongLong(),first["mid"].toInt());if(first["kind"].toString()=="edit"){if(!state.edits.isEmpty())state.edits.removeFirst();}else{state.deleted.remove(key);state.deletedAt.remove(key);state.order.removeOne(key);}clear=QJsonDocument(records).toJson(QJsonDocument::Compact);}
  const auto encrypted=Protect(clear,i.key(),true);if(encrypted.isEmpty())continue;QSaveFile file(Path(i.key()));if(file.open(QIODevice::WriteOnly)&&file.write(encrypted)==encrypted.size()&&file.commit())state.dirty=false;
 }
}
QByteArray Semantic(const MTPMessage &message){mtpBuffer b;const auto &m=message.c_message();m.vmessage().write(b);if(const auto entities=m.ventities())entities->write(b);if(const auto media=m.vmedia())media->write(b);return QByteArray(reinterpret_cast<const char*>(b.data()),int(b.size()*sizeof(mtpPrime)));}
void CaptureMessage(quint64 owner,quint64 peer,const MTPMessage &message){
 if(!owner)return;
 if(!Eligible(message)){
  if(message.type()==mtpc_message&&IdFromMessage(message).bare>0){auto &state=Read(owner);const auto key=Key(peer,IdFromMessage(message).bare);state.snapshots.remove(key);const auto removed=state.deleted.remove(key);state.deletedAt.remove(key);state.order.removeAll(key);const auto before=state.edits.size();state.edits.erase(std::remove_if(state.edits.begin(),state.edits.end(),[&](const Version &v){return v.key==key;}),state.edits.end());if(removed||before!=state.edits.size()){state.dirty=true;ScheduleFlush();}}return;
 }
 auto &state=Read(owner);const auto body=Encode(message);if(body.size()>512*1024)return;
 const auto key=Key(peer,IdFromMessage(message).bare);
 if(Policy(owner,"keepEdits")){MTPMessage old;const auto previous=state.snapshots.value(key);if(previous!=body&&Decode(previous,old)&&Semantic(old)!=Semantic(message)){state.edits.append({key,previous,QDateTime::currentSecsSinceEpoch()});while(state.edits.size()>3000)state.edits.removeFirst();state.dirty=true;ScheduleFlush();}}
 state.snapshots.insert(key,body);
 int bytes=0;for(const auto &v:state.snapshots)bytes+=v.size();
 while(state.snapshots.size()>kLimit||bytes>kMaxBytes){auto i=state.snapshots.begin();bytes-=i.value().size();state.snapshots.erase(i);}
}
bool IsDeletedCopy(quint64 owner,quint64 peer,int mid){return Read(owner).deleted.contains(Key(peer,mid));}
bool RetainDeletedMessage(HistoryItem *item){
 if(!item||!item->isRegular()||item->ttlDestroyAt()!=0)return false;
 const auto owner=item->history()->session().uniqueId();if(!KeepDeletedMessages(owner))return false;auto &state=Read(owner);const auto key=Key(item->history()->peer->id.value,item->id.bare);
 const auto body=state.snapshots.value(key,state.deleted.value(key));MTPMessage m;if(!Decode(body,m))return false;
 state.deleted.insert(key,body);state.deletedAt.insert(key,QDateTime::currentSecsSinceEpoch());state.order.removeOne(key);state.order.append(key);while(state.order.size()>kLimit)state.deleted.remove(state.order.takeFirst());state.dirty=true;ScheduleFlush();
 if(!ArchiveAllowed(owner))return false;
 if(!item->nyxDeleted)item->history()->itemVanished(item);item->nyxDeleted=true;item->history()->owner().notifyItemDataChange(item);item->history()->owner().requestItemResize(item);item->history()->owner().requestItemRepaint(item);
 // Let the native API refresh the authoritative last message / dialog counters.
 item->history()->requestChatListMessage();return true;
}
QVector<MTPMessage> MergeDeletedMessages(History *history,const QVector<MTPMessage> &source){
 if(!history||!KeepDeletedMessages(history->session().uniqueId())||!ArchiveAllowed(history->session().uniqueId()))return source;
 const auto owner=history->session().uniqueId(),peer=history->peer->id.value;auto &state=Read(owner);Prune(owner,state);QVector<MTPMessage> result=source;
 int min=0x7fffffff,max=0;QSet<int> ids;for(const auto &m:source){const auto id=IdFromMessage(m).bare;if(id>0){min=qMin(min,int(id));max=qMax(max,int(id));ids.insert(id);}}
 const auto latest=history->lastMessageKnown()&&(!history->lastMessage()||history->lastMessage()->id.bare<=max);
 int added=0;for(auto it=state.order.crbegin();it!=state.order.crend()&&added<100;++it){const auto parts=it->split(':');if(parts[0].toULongLong()!=peer)continue;const auto mid=parts[1].toInt();if(ids.contains(mid)||(!source.isEmpty()&&(mid<min||(!latest&&mid>max))))continue;MTPMessage m;if(Decode(state.deleted.value(*it),m)){result.append(m);++added;}}
 std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return IdFromMessage(a).bare>IdFromMessage(b).bare;});return result;
}
void ShowLocalHistory(Main::Session *session,quint64 peer,int mid){
 if(!session||!UnlockArchive(session))return;const auto owner=session->uniqueId();const auto guard=base::make_weak(session);auto dialog=new QDialog(QApplication::activeWindow());dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(QString::fromUtf8("Локальная история · Nyxgram"));dialog->resize(650,650);auto layout=new QVBoxLayout(dialog);auto search=new QLineEdit;search->setPlaceholderText(QString::fromUtf8("Поиск по сохранённым сообщениям"));layout->addWidget(search);auto text=new QTextEdit;text->setReadOnly(true);layout->addWidget(text);const auto update=[=](const QString &query){if(!guard||ActiveFeatureSession()!=guard.get()||!ArchiveAllowed(owner)){dialog->close();return;}auto &state=Read(owner);Prune(owner,state);QStringList lines;auto add=[&](const QString &key,const QByteArray &body,const QString &kind){const auto parts=key.split(':');if(peer&&parts[0].toULongLong()!=peer||mid&&parts[1].toInt()!=mid)return;MTPMessage m;if(!Decode(body,m))return;const auto value=qs(m.c_message().vmessage());if(!query.isEmpty()&&!value.contains(query,Qt::CaseInsensitive))return;lines<<kind+" · "+key+"\n"+value;};for(auto i=state.edits.crbegin();i!=state.edits.crend()&&lines.size()<1000;++i)add(i->key,i->body,QString::fromUtf8("Предыдущая версия"));for(auto i=state.order.crbegin();i!=state.order.crend()&&lines.size()<1000;++i)add(*i,state.deleted.value(*i),QString::fromUtf8("Удалено"));text->setPlainText(lines.join("\n\n────────────\n\n"));};QObject::connect(search,&QLineEdit::textChanged,dialog,update);auto timer=new QTimer(dialog);QObject::connect(timer,&QTimer::timeout,dialog,[=]{if(!guard||ActiveFeatureSession()!=guard.get()||!ArchiveAllowed(owner)){text->clear();dialog->close();}});timer->start(500);update({});dialog->show();
}

namespace {
QHash<quint64,QJsonObject> emojiMetadata;
QString EmojiPath(quint64 owner){return cWorkingDir()+QString("tdata/nyx/emoji-%1.bin").arg(owner);}
QJsonObject &EmojiMetadata(quint64 owner){
 if(!emojiMetadata.contains(owner)){QFile file(EmojiPath(owner));QJsonObject entries;if(file.size()<=1024*1024&&file.open(QIODevice::ReadOnly))entries=QJsonDocument::fromJson(Protect(file.readAll(),owner,false)).object();if(entries.size()>1000)entries={};emojiMetadata.insert(owner,entries);}return emojiMetadata[owner];
}
void WriteEmojiMetadata(quint64 owner){
 auto &entries=EmojiMetadata(owner);while(entries.size()>1000)entries.erase(entries.begin());auto clear=QJsonDocument(entries).toJson(QJsonDocument::Compact);while(clear.size()>768*1024&&!entries.isEmpty()){entries.erase(entries.begin());clear=QJsonDocument(entries).toJson(QJsonDocument::Compact);}const auto encrypted=Protect(clear,owner,true);if(encrypted.isEmpty())return;QDir().mkpath(cWorkingDir()+QString("tdata/nyx"));QSaveFile file(EmojiPath(owner));if(file.open(QIODevice::WriteOnly)&&file.write(encrypted)==encrypted.size())file.commit();
}
}
void RememberLocalEmoji(HistoryItem *item){
 if(!item||!item->out()||!item->isRegular()||item->id.bare<=0||item->ttlDestroyAt())return;
 const auto owner=item->history()->session().uniqueId();if(!LocalPremium(owner)||item->history()->session().premium())return;
 const auto &text=item->originalText();QJsonArray entities;
 for(const auto &e:text.entities)if(e.type()==EntityType::CustomEmoji&&e.offset()>=0&&e.length()>0&&e.offset()+e.length()<=text.text.size()){
  QJsonObject v;v["offset"]=e.offset();v["length"]=e.length();v["data"]=e.data();entities.append(v);if(entities.size()>=100)break;
 }
 if(entities.isEmpty())return;QJsonObject record;record["hash"]=QString::fromLatin1(QCryptographicHash::hash(text.text.toUtf8(),QCryptographicHash::Sha256).toHex());record["entities"]=entities;
 auto &entries=EmojiMetadata(owner);const auto key=Key(item->history()->peer->id.value,item->id.bare);if(entries.value(key).toObject()==record)return;entries[key]=record;WriteEmojiMetadata(owner);
}
void RestoreLocalEmoji(HistoryItem *item){
 if(!item||!item->out()||!item->isRegular()||item->id.bare<=0)return;
 const auto owner=item->history()->session().uniqueId();if(!LocalPremium(owner)||item->history()->session().premium())return;
 auto &entries=EmojiMetadata(owner);const auto key=Key(item->history()->peer->id.value,item->id.bare);const auto record=entries.value(key).toObject();if(record.isEmpty())return;
 auto text=item->originalText();if(item->ttlDestroyAt()||record["hash"].toString().toLatin1()!=QCryptographicHash::hash(text.text.toUtf8(),QCryptographicHash::Sha256).toHex()){entries.remove(key);WriteEmojiMetadata(owner);return;}
 for(const auto &e:text.entities)if(e.type()==EntityType::CustomEmoji)return;
 const auto entities=record["entities"].toArray();if(entities.size()>100)return;
 for(const auto &v:entities){const auto e=v.toObject();const auto offset=e["offset"].toInt(-1),length=e["length"].toInt();const auto data=e["data"].toString();if(offset<0||length<=0||offset>text.text.size()||length>text.text.size()-offset||data.isEmpty()||data.size()>100)return;}
 for(const auto &v:entities){const auto e=v.toObject();text.entities.append(EntityInText(EntityType::CustomEmoji,e["offset"].toInt(),e["length"].toInt(),e["data"].toString()));}
 item->setText(text);item->history()->owner().requestItemTextRefresh(item);
}

}
