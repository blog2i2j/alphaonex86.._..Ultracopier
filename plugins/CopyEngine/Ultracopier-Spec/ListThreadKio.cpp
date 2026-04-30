/** \file ListThreadKio.cpp
\brief KIO integration for remote file transfers
\author alpha_one_x86
\licence GPL3, see the file COPYING */

#ifdef ULTRACOPIER_PLUGIN_KIO

#include "ListThread.h"
#include "../../../cpp11addition.h"

#include <QUrl>
#include <KIO/CopyJob>
#include <KJob>

bool ListThread::hasRemoteUrl(const std::vector<std::string> &sources,const std::string &destination)
{
    // Check destination
    {
        const QUrl url=QUrl(QString::fromStdString(destination));
        const QString &scheme=url.scheme();
        if(!scheme.isEmpty() && scheme!=QStringLiteral("file"))
            return true;
    }
    // Check sources
    for(const auto &source : sources)
    {
        const QUrl url=QUrl(QString::fromStdString(source));
        const QString &scheme=url.scheme();
        if(!scheme.isEmpty() && scheme!=QStringLiteral("file"))
            return true;
    }
    return false;
}

bool ListThread::newCopyKio(const std::vector<std::string> &sources,const std::string &destination)
{
    return startKioJob(sources,destination,Ultracopier::Copy);
}

bool ListThread::newMoveKio(const std::vector<std::string> &sources,const std::string &destination)
{
    return startKioJob(sources,destination,Ultracopier::Move);
}

bool ListThread::startKioJob(const std::vector<std::string> &sources,const std::string &destination,Ultracopier::CopyMode mode)
{
    ULTRACOPIER_DEBUGCONSOLE(Ultracopier::DebugLevel_Notice,"start KIO job, sources: "+stringimplode(sources,";")+", destination: "+destination);

    QList<QUrl> sourceUrls;
    for(const auto &source : sources)
    {
        QUrl url=QUrl(QString::fromStdString(source));
        // Plain path without scheme -> local file
        if(url.scheme().isEmpty())
            url=QUrl::fromLocalFile(QString::fromStdString(source));
        sourceUrls.append(url);
    }

    QUrl destUrl=QUrl(QString::fromStdString(destination));
    if(destUrl.scheme().isEmpty())
        destUrl=QUrl::fromLocalFile(QString::fromStdString(destination));

    KIO::CopyJob *job=nullptr;
    if(mode==Ultracopier::Copy)
        job=KIO::copy(sourceUrls,destUrl,KIO::DefaultFlags);
    else
        job=KIO::move(sourceUrls,destUrl,KIO::DefaultFlags);

    if(!job)
    {
        ULTRACOPIER_DEBUGCONSOLE(Ultracopier::DebugLevel_Warning,"KIO job creation failed");
        return false;
    }

    // Disable KIO's own UI (Ultracopier provides the UI)
    job->setUiDelegate(nullptr);

    connect(job,&KJob::result,this,&ListThread::kioJobResult);
    // KF6 renamed processedAmount -> processedAmountChanged, totalAmount -> totalAmountChanged
    #if QT_VERSION_MAJOR >= 6
    connect(job,&KJob::processedAmountChanged,this,&ListThread::kioJobProcessedAmount);
    connect(job,&KJob::totalAmountChanged,this,&ListThread::kioJobTotalAmount);
    #else
    connect(job,SIGNAL(processedAmount(KJob*,KJob::Unit,qulonglong)),
            this,SLOT(kioJobProcessedAmount(KJob*,KJob::Unit,qulonglong)));
    connect(job,SIGNAL(totalAmount(KJob*,KJob::Unit,qulonglong)),
            this,SLOT(kioJobTotalAmount(KJob*,KJob::Unit,qulonglong)));
    #endif

    KioJobEntry entry;
    entry.job=job;
    entry.totalBytes=0;
    entry.processedBytes=0;
    kioJobs.push_back(entry);

    emit actionInProgess(Ultracopier::CopyingAndListing);

    job->start();
    return true;
}

void ListThread::kioJobResult(KJob *job)
{
    for(auto it=kioJobs.begin();it!=kioJobs.end();++it)
    {
        if(it->job==job)
        {
            if(job->error())
            {
                ULTRACOPIER_DEBUGCONSOLE(Ultracopier::DebugLevel_Warning,
                    "KIO job failed: "+job->errorString().toStdString());
                emit error("",0,0,job->errorString().toStdString());
            }
            else
            {
                ULTRACOPIER_DEBUGCONSOLE(Ultracopier::DebugLevel_Notice,"KIO job finished successfully");
            }
            kioJobs.erase(it);
            break;
        }
    }

    if(kioJobs.empty())
    {
        emit actionInProgess(Ultracopier::Idle);
        updateTheStatus();
    }
}

void ListThread::kioJobProcessedAmount(KJob *job,KJob::Unit unit,qulonglong amount)
{
    if(unit!=KJob::Bytes)
        return;
    for(auto &entry : kioJobs)
    {
        if(entry.job==job)
        {
            entry.processedBytes=amount;
            break;
        }
    }
    // Emit combined progression across all KIO jobs
    uint64_t totalProcessed=0;
    uint64_t totalSize=0;
    for(const auto &entry : kioJobs)
    {
        totalProcessed+=entry.processedBytes;
        totalSize+=entry.totalBytes;
    }
    emit pushGeneralProgression(totalProcessed,totalSize);
}

void ListThread::kioJobTotalAmount(KJob *job,KJob::Unit unit,qulonglong amount)
{
    if(unit!=KJob::Bytes)
        return;
    for(auto &entry : kioJobs)
    {
        if(entry.job==job)
        {
            entry.totalBytes=amount;
            break;
        }
    }
}

#endif // ULTRACOPIER_PLUGIN_KIO
