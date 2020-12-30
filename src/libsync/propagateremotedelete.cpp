/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "propagateremotedelete.h"
#include "propagateremotedeleteencrypted.h"
#include "owncloudpropagator_p.h"
#include "account.h"
#include "common/asserts.h"

#include "clientsideencryptionjobs.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcDeleteJob, "nextcloud.sync.networkjob.delete", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropagateRemoteDelete, "nextcloud.sync.propagator.remotedelete", QtInfoMsg)

DeleteJob::DeleteJob(AccountPtr account, const QString &path, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
{
}

DeleteJob::DeleteJob(AccountPtr account, const QUrl &url, QObject *parent)
    : AbstractNetworkJob(account, QString(), parent)
    , _url(url)
{
}

void DeleteJob::start()
{
    QNetworkRequest req;
    if (!_folderToken.isEmpty()) {
        req.setRawHeader("e2e-token", _folderToken);
    }

    if (_url.isValid()) {
        sendRequest("DELETE", _url, req);
    } else {
        sendRequest("DELETE", makeDavUrl(path()), req);
    }

    if (reply()->error() != QNetworkReply::NoError) {
        qCWarning(lcDeleteJob) << " Network error: " << reply()->errorString();
    }
    AbstractNetworkJob::start();
}

bool DeleteJob::finished()
{
    qCInfo(lcDeleteJob) << "DELETE of" << reply()->request().url() << "FINISHED WITH STATUS"
                       << replyStatusString();

    emit finishedSignal();
    return true;
}

QByteArray DeleteJob::folderToken() const
{
    return _folderToken;
}

void DeleteJob::setFolderToken(const QByteArray &folderToken)
{
    _folderToken = folderToken;
}

PropagatorJob::JobParallelism PropagateRemoteDelete::parallelism()
{
    return _item->_encryptedFileName.isEmpty() ? FullParallelism : WaitForFinished;
}

void PropagateRemoteDelete::start()
{
    if (propagator()->_abortRequested)
        return;

    if (!_item->_encryptedFileName.isEmpty()) {
        _deleteEncryptedHelper = new PropagateRemoteDeleteEncrypted(propagator(), _item, this);
        connect(_deleteEncryptedHelper, &PropagateRemoteDeleteEncrypted::finished, this, [this] (bool success) {
            Q_UNUSED(success) // Should we skip file deletion in case of failure?
            createDeleteJob(_item->_encryptedFileName);
        });
        _deleteEncryptedHelper->start();
    } else if (_item->_isEncrypted) {
        auto lockJob = new LockEncryptFolderApiJob(propagator()->account(), _item->_fileId, this);
        connect(lockJob, &LockEncryptFolderApiJob::success, this, [=] (const QByteArray& fileId, const QByteArray& token) {
            createDeleteJob(_item->_file);
        });
        connect(lockJob, &LockEncryptFolderApiJob::error, this, [=] (const QByteArray& fileId, int httpdErrorCode) {
            createDeleteJob(_item->_file);
        });
        lockJob->start();
    }
    else {
        createDeleteJob(_item->_file);
    }
}

void PropagateRemoteDelete::createDeleteJob(const QString &filename)
{
    qCInfo(lcPropagateRemoteDelete) << "Deleting file, local" << _item->_file << "remote" << filename;

    _job = new DeleteJob(propagator()->account(),
        propagator()->fullRemotePath(filename),
        this);
    if (_deleteEncryptedHelper && !_deleteEncryptedHelper->folderToken().isEmpty()) {
        _job->setFolderToken(_deleteEncryptedHelper->folderToken());
    }
    connect(_job.data(), &DeleteJob::finishedSignal, this, &PropagateRemoteDelete::slotDeleteJobFinished);
    propagator()->_activeJobList.append(this);
    _job->start();
}

void PropagateRemoteDelete::abort(PropagatorJob::AbortType abortType)
{
    if (_job && _job->reply())
        _job->reply()->abort();

    if (abortType == AbortType::Asynchronous) {
        emit abortFinished();
    }
}

void PropagateRemoteDelete::slotDeleteJobFinished()
{
    propagator()->_activeJobList.removeOne(this);

    ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    const int httpStatus = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_httpErrorCode = httpStatus;
    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_requestId = _job->requestId();

    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);
        done(status, _job->errorString());
        return;
    }

    // A 404 reply is also considered a success here: We want to make sure
    // a file is gone from the server. It not being there in the first place
    // is ok. This will happen for files that are in the DB but not on
    // the server or the local file system.
    if (httpStatus != 204 && httpStatus != 404) {
        // Normally we expect "204 No Content"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError,
            tr("Wrong HTTP code returned by server. Expected 204, but received \"%1 %2\".")
                .arg(_item->_httpErrorCode)
                .arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        return;
    }

    propagator()->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory());
    propagator()->_journal->commit("Remote Remove");

    if (_deleteEncryptedHelper && !_job->folderToken().isEmpty()) {
        propagator()->_activeJobList.append(this);
        connect(_deleteEncryptedHelper, &PropagateRemoteDeleteEncrypted::folderUnlocked,
                this, [this] {
            propagator()->_activeJobList.removeOne(this);
            done(SyncFileItem::Success);
        });
        _deleteEncryptedHelper->unlockFolder();
    } else {
        done(SyncFileItem::Success);
    }
}
}
