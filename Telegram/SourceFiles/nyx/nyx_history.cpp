// Owner-scoped local copies. Native deletion updates and PTS processing are not suppressed.
#include "nyx/nyx_history.h"
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
struct Archive { QHash<QString,QByteArray> snapshots, deleted; QStringList order; bool dirty=false; };
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
    if(peer&&mid>0&&Decode(body,m)&&PeerFromMessage(m).value==peer&&IdFromMessage(m).bare==mid&&state.deleted.size()<kLimit){auto key=Key(peer,mid);state.deleted.insert(key,body);state.order.append(key);}
   }
  }
  archives.insert(owner,state);
 }
 return archives[owner];
}
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
 if(!session)return;HideDeletedCopies(session);const auto owner=session->uniqueId();auto &state=Read(owner);state.deleted.clear();state.order.clear();state.dirty=false;QFile::remove(Path(owner));
}
void RefreshDeletedCopies(Main::Session *session){
 if(!session)return;const auto keys=Read(session->uniqueId()).order;for(const auto &key:keys){const auto parts=key.split(':');if(const auto item=session->data().message(PeerId(parts[0].toULongLong()),MsgId(parts[1].toLongLong()));item&&item->nyxDeleted){session->data().notifyItemDataChange(item);session->data().requestItemResize(item);session->data().requestItemRepaint(item);}}
}
void FlushDeletedCopies(){
 QDir().mkpath(cWorkingDir()+QString("tdata/nyx/"));
 for(auto i=archives.begin();i!=archives.end();++i){auto &state=i.value();if(!state.dirty)continue;QJsonArray records;
  for(const auto &key:state.order){const auto body=state.deleted.value(key);if(body.isEmpty())continue;const auto parts=key.split(':');QJsonObject o;o["peer"]=parts[0];o["mid"]=parts[1].toInt();o["body"]=QString::fromLatin1(body.toBase64());records.append(o);}
  auto clear=QJsonDocument(records).toJson(QJsonDocument::Compact);
  while(clear.size()>kMaxBytes&&!records.isEmpty()){const auto first=records.takeAt(0).toObject();const auto key=Key(first["peer"].toString().toULongLong(),first["mid"].toInt());state.deleted.remove(key);state.order.removeOne(key);clear=QJsonDocument(records).toJson(QJsonDocument::Compact);}
  const auto encrypted=Protect(clear,i.key(),true);if(encrypted.isEmpty())continue;QSaveFile file(Path(i.key()));if(file.open(QIODevice::WriteOnly)&&file.write(encrypted)==encrypted.size()&&file.commit())state.dirty=false;
 }
}
void CaptureMessage(quint64 owner,quint64 peer,const MTPMessage &message){
 if(!owner||!Eligible(message))return;
 auto &state=Read(owner);const auto body=Encode(message);if(body.size()>512*1024)return;
 state.snapshots.insert(Key(peer,IdFromMessage(message).bare),body);
 int bytes=0;for(const auto &v:state.snapshots)bytes+=v.size();
 while(state.snapshots.size()>kLimit||bytes>kMaxBytes){auto i=state.snapshots.begin();bytes-=i.value().size();state.snapshots.erase(i);}
}
bool IsDeletedCopy(quint64 owner,quint64 peer,int mid){return Read(owner).deleted.contains(Key(peer,mid));}
bool RetainDeletedMessage(HistoryItem *item){
 if(!item||!KeepDeletedMessages()||!item->isRegular()||item->ttlDestroyAt()!=0)return false;
 const auto owner=item->history()->session().uniqueId();auto &state=Read(owner);const auto key=Key(item->history()->peer->id.value,item->id.bare);
 const auto body=state.snapshots.value(key,state.deleted.value(key));MTPMessage m;if(!Decode(body,m))return false;
 state.deleted.insert(key,body);state.order.removeOne(key);state.order.append(key);while(state.order.size()>kLimit)state.deleted.remove(state.order.takeFirst());state.dirty=true;ScheduleFlush();
 if(!item->nyxDeleted)item->history()->itemVanished(item);item->nyxDeleted=true;item->history()->owner().notifyItemDataChange(item);item->history()->owner().requestItemResize(item);item->history()->owner().requestItemRepaint(item);
 // Let the native API refresh the authoritative last message / dialog counters.
 item->history()->requestChatListMessage();return true;
}
QVector<MTPMessage> MergeDeletedMessages(History *history,const QVector<MTPMessage> &source){
 if(!KeepDeletedMessages()||!history)return source;
 const auto owner=history->session().uniqueId(),peer=history->peer->id.value;auto &state=Read(owner);QVector<MTPMessage> result=source;
 int min=0x7fffffff,max=0;QSet<int> ids;for(const auto &m:source){const auto id=IdFromMessage(m).bare;if(id>0){min=qMin(min,int(id));max=qMax(max,int(id));ids.insert(id);}}
 const auto latest=history->lastMessageKnown()&&(!history->lastMessage()||history->lastMessage()->id.bare<=max);
 int added=0;for(auto it=state.order.crbegin();it!=state.order.crend()&&added<100;++it){const auto parts=it->split(':');if(parts[0].toULongLong()!=peer)continue;const auto mid=parts[1].toInt();if(ids.contains(mid)||(!source.isEmpty()&&(mid<min||(!latest&&mid>max))))continue;MTPMessage m;if(Decode(state.deleted.value(*it),m)){result.append(m);++added;}}
 std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return IdFromMessage(a).bare>IdFromMessage(b).bare;});return result;
}
}
