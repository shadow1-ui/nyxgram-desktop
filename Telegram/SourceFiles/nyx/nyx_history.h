#pragma once
#include "mtproto/scheme.h"
namespace Main {class Session;}
class History;
class HistoryItem;
namespace Nyxgram {
void CaptureMessage(quint64 owner, quint64 peer, const MTPMessage &message);
bool RetainDeletedMessage(HistoryItem *item);
bool IsDeletedCopy(quint64 owner, quint64 peer, int mid);
QVector<MTPMessage> MergeDeletedMessages(History *history, const QVector<MTPMessage> &source);
void HideDeletedCopies(Main::Session *session);
void FlushDeletedCopies();
void ClearDeletedCopies(Main::Session *session);
void RefreshDeletedCopies(Main::Session *session);
}
