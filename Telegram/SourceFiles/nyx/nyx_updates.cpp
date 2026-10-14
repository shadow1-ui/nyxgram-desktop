// Fork-specific portable update downloader. Never executes or replaces a running EXE.
#include "nyx/nyx_features.h"
#include "main/main_session.h"
#include "core/application.h"
#include "settings.h"
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QSettings>
#include <QtCore/QDateTime>
#include <QtCore/QCryptographicHash>
#include <QtCore/QSaveFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#ifndef NYXGRAM_BUILD_NUMBER
#define NYXGRAM_BUILD_NUMBER 0
#endif
namespace Nyxgram {
namespace {
bool checking=false;
bool Allowed(QUrl u){const auto h=u.host().toLower();return u.scheme()=="https"&&(h=="api.github.com"||h=="github.com"||h.endsWith(".github.com")||h.endsWith(".githubusercontent.com"));}
void Fetch(QUrl url,qint64 limit,Fn<void(QByteArray,int)> done,int redirects=0){
 if(!Allowed(url)||redirects>5){done({},0);return;}static auto manager=new QNetworkAccessManager(qApp);QNetworkRequest request(url);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setRawHeader("User-Agent","Nyxgram-Desktop/0.3.0");request.setRawHeader("Accept","application/vnd.github+json");request.setTransferTimeout(60000);auto reply=manager->get(request);auto body=std::make_shared<QByteArray>();QObject::connect(reply,&QNetworkReply::readyRead,reply,[=]{body->append(reply->readAll());if(body->size()>limit){body->clear();reply->abort();}});QObject::connect(reply,&QNetworkReply::finished,reply,[=]{const auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto next=reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();body->append(reply->readAll());const auto ok=reply->error()==QNetworkReply::NoError&&body->size()<=limit;reply->deleteLater();if(code>=300&&code<400&&!next.isEmpty()){Fetch(url.resolved(next),limit,done,redirects+1);return;}done(ok&&code==200?*body:QByteArray(),code);});
}
}
void CheckDesktopUpdates(bool manual){
 if(checking)return;const auto session=ActiveFeatureSession();if(!session)return;const auto owner=base::make_weak(session);if(!manual&&!Policy(session->uniqueId(),"autoUpdates",true))return;QSettings settings(cWorkingDir()+QString("tdata/nyx/preferences.ini"),QSettings::IniFormat);const auto now=QDateTime::currentSecsSinceEpoch();if(!manual&&now-settings.value("desktopLastUpdateCheck",0).toLongLong()<12*60*60)return;settings.setValue("desktopLastUpdateCheck",now);checking=true;
 Fetch(QUrl("https://api.github.com/repos/shadow1-ui/nyxgram-desktop/releases/latest"),512000,[=](QByteArray data,int code){if(!owner||ActiveFeatureSession()!=owner.get()){checking=false;return;}if(data.isEmpty()){checking=false;if(manual)QMessageBox::information(QApplication::activeWindow(),"Nyxgram",QString::fromUtf8(code==404?"Опубликованных обновлений пока нет.":"Не удалось проверить обновления. Попробуйте позже."));return;}const auto release=QJsonDocument::fromJson(data).object();if(release["draft"].toBool()||release["prerelease"].toBool()){checking=false;return;}QUrl manifest,archive;QString archiveName;for(auto v:release["assets"].toArray()){const auto a=v.toObject();const auto name=a["name"].toString();if(name=="nyx-desktop-update.json")manifest=QUrl(a["browser_download_url"].toString());if(name.endsWith("-Windows-x64.zip")){archive=QUrl(a["browser_download_url"].toString());archiveName=name;}}if(manifest.isEmpty()||archive.isEmpty()){checking=false;return;}
 Fetch(manifest,65536,[=](QByteArray bytes,int){checking=false;if(!owner||ActiveFeatureSession()!=owner.get())return;const auto m=QJsonDocument::fromJson(bytes).object();const auto size=m["size"].toVariant().toLongLong();const auto sha=m["sha256"].toString().toLatin1();if(m["package"].toString()!="com.nyxgram.desktop"||m["archive"].toString()!=archiveName||size<=0||size>400LL*1024*1024||sha.size()!=64)return;if(m["versionCode"].toInt()<=NYXGRAM_BUILD_NUMBER){if(manual)QMessageBox::information(QApplication::activeWindow(),"Nyxgram",QString::fromUtf8("У Вас установлена актуальная версия."));return;}if(QMessageBox::question(QApplication::activeWindow(),QString::fromUtf8("Обновление Nyxgram"),release["name"].toString()+"\n\n"+release["body"].toString().left(4000)+QString::fromUtf8("\n\nСкачать архив Windows? Существующая папка tdata не удаляется."))!=QMessageBox::Yes)return;const auto path=QFileDialog::getSaveFileName(QApplication::activeWindow(),QString::fromUtf8("Сохранить обновление"),archiveName,"ZIP (*.zip)");if(path.isEmpty())return;Fetch(archive,size,[=](QByteArray payload,int){if(!owner||ActiveFeatureSession()!=owner.get())return;if(payload.size()!=size||QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex()!=sha.toLower()){QMessageBox::warning(QApplication::activeWindow(),"Nyxgram",QString::fromUtf8("Обновление не сохранено: размер или контрольная сумма не совпали."));return;}QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(payload)!=payload.size()||!file.commit())return;QMessageBox::information(QApplication::activeWindow(),"Nyxgram",QString::fromUtf8("Архив проверен и сохранён. Закройте Nyxgram и распакуйте обновление поверх его отдельной папки, сохранив tdata. Не распаковывайте в папку официального Telegram."));QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));});});});
}
}
