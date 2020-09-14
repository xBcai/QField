/***************************************************************************
    qfieldcloudprojectsmodel.cpp
    ---------------------
    begin                : January 2020
    copyright            : (C) 2020 by Matthias Kuhn
    email                : matthias at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qfieldcloudprojectsmodel.h"
#include "qfieldcloudconnection.h"
#include "qfieldcloudutils.h"
#include "layerobserver.h"
#include "deltafilewrapper.h"

#include <qgis.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsapplication.h>
#include <qgsproject.h>
#include <qgsproviderregistry.h>

#include <QNetworkReply>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QDirIterator>
#include <QSettings>
#include <QDebug>

QFieldCloudProjectsModel::QFieldCloudProjectsModel()
{
  QJsonArray projects;
  reload( projects );

  // TODO all of these connects are a bit too much, and I guess not very precise, should be refactored!
  connect( this, &QFieldCloudProjectsModel::currentProjectIdChanged, this, [ = ]()
  {
    const int index = findProject( mCurrentProjectId );

    if ( index == -1 || index >= mCloudProjects.size() )
      return;

    refreshProjectModification( mCurrentProjectId );

    updateCanCommitCurrentProject();
    updateCanSyncCurrentProject();
  } );

  connect( this, &QFieldCloudProjectsModel::modelReset, this, [ = ]()
  {
    const int index = findProject( mCurrentProjectId );

    if ( index == -1 || index >= mCloudProjects.size() )
      return;

    refreshProjectModification( mCurrentProjectId );

    updateCanCommitCurrentProject();
    updateCanSyncCurrentProject();
  } );

  connect( this, &QFieldCloudProjectsModel::dataChanged, this, [ = ]( const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> &roles )
  {
    Q_UNUSED( bottomRight );

    const int index = findProject( mCurrentProjectId );

    if ( index == -1 || index >= mCloudProjects.size() )
      return;

    updateCanCommitCurrentProject();
    updateCanSyncCurrentProject();

    // current project
    if ( topLeft.row() == index )
    {
      if ( roles.contains( static_cast<int>( ColumnRole::StatusRole ) ) )
      {
        emit currentProjectStatusChanged();
      }
    }
  } );
}

QFieldCloudConnection *QFieldCloudProjectsModel::cloudConnection() const
{
  return mCloudConnection;
}

void QFieldCloudProjectsModel::setCloudConnection( QFieldCloudConnection *cloudConnection )
{
  if ( mCloudConnection == cloudConnection )
    return;

  if ( cloudConnection )
    connect( cloudConnection, &QFieldCloudConnection::statusChanged, this, &QFieldCloudProjectsModel::connectionStatusChanged );

  mCloudConnection = cloudConnection;
  emit cloudConnectionChanged();
}

LayerObserver *QFieldCloudProjectsModel::layerObserver() const
{
  return mLayerObserver;
}

void QFieldCloudProjectsModel::setLayerObserver( LayerObserver *layerObserver )
{
  if ( mLayerObserver == layerObserver )
    return;

  mLayerObserver = layerObserver;

  if ( ! layerObserver )
    return;

  connect( layerObserver, &LayerObserver::layerEdited, this, &QFieldCloudProjectsModel::layerObserverLayerEdited );
  connect( layerObserver, &LayerObserver::deltaFileCommitted, this, [ = ]()
  {
    refreshProjectModification( mCurrentProjectId );
    updateCanCommitCurrentProject();
    updateCanSyncCurrentProject();
  } );

  emit layerObserverChanged();
}

QString QFieldCloudProjectsModel::currentProjectId() const
{
  return mCurrentProjectId;
}

void QFieldCloudProjectsModel::setCurrentProjectId( const QString &currentProjectId )
{
  if ( mCurrentProjectId == currentProjectId )
    return;

  mCurrentProjectId = currentProjectId;
  emit currentProjectIdChanged();
}

QFieldCloudProjectsModel::ProjectStatus QFieldCloudProjectsModel::currentProjectStatus() const
{
  const int index = findProject( mCurrentProjectId );

  if ( index == -1 || index >= mCloudProjects.size() )
    return ProjectStatus::Idle;

  return mCloudProjects[index].status;
}

void QFieldCloudProjectsModel::refreshProjectsList()
{
  switch ( mCloudConnection->status() )
  {
    case QFieldCloudConnection::ConnectionStatus::LoggedIn:
    {
      NetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/projects/" ) );
      connect( reply, &NetworkReply::finished, this, &QFieldCloudProjectsModel::projectListReceived );
      break;
    }
    case QFieldCloudConnection::ConnectionStatus::Disconnected:
    {
      QJsonArray projects;
      reload( projects );
      break;
    }
    case QFieldCloudConnection::ConnectionStatus::Connecting:
      // Nothing done for this intermediary status.
      break;
  }
}

int QFieldCloudProjectsModel::findProject( const QString &projectId ) const
{
  const QList<CloudProject> cloudProjects = mCloudProjects;
  int index = -1;
  for ( int i = 0; i < cloudProjects.count(); i++ )
  {
    if ( cloudProjects.at( i ).id == projectId )
    {
      index = i;
      break;
    }
  }
  return index;
}

void QFieldCloudProjectsModel::removeLocalProject( const QString &projectId )
{
  QDir dir( QStringLiteral( "%1/%2/" ).arg( QFieldCloudUtils::localCloudDirectory(), projectId ) );

  if ( dir.exists() )
  {
    int index = findProject( projectId );
    if ( index > -1 )
    {
      if ( mCloudProjects.at( index ).status == ProjectStatus::Idle && mCloudProjects.at( index ).checkout & RemoteCheckout )
      {
        mCloudProjects[index].localPath = QString();
        mCloudProjects[index].checkout = RemoteCheckout;
        mCloudProjects[index].modification = NoModification;
        QModelIndex idx = createIndex( index, 0 );
        emit dataChanged( idx, idx,  QVector<int>() << StatusRole << LocalPathRole << CheckoutRole );
      }
      else
      {
        beginRemoveRows( QModelIndex(), index, index );
        mCloudProjects.removeAt( index );
        endRemoveRows();
      }
    }

    dir.removeRecursively();
  }

  QSettings().remove( QStringLiteral( "QFieldCloud/projects/%1" ).arg( projectId ) );
}

QFieldCloudProjectsModel::ProjectStatus QFieldCloudProjectsModel::projectStatus( const QString &projectId )
{
  const int index = findProject( projectId );

  if ( index == -1 || index >= mCloudProjects.size() )
    return QFieldCloudProjectsModel::ProjectStatus::Idle;

  return mCloudProjects[index].status;
}

bool QFieldCloudProjectsModel::canCommitCurrentProject()
{
  return mCanCommitCurrentProject;
}

void QFieldCloudProjectsModel::updateCanCommitCurrentProject()
{
  bool oldCanCommitCurrentProject = mCanCommitCurrentProject;

  if ( mCurrentProjectId.isEmpty() )
  {
    mCanCommitCurrentProject = false;
  }
  else if ( projectStatus( mCurrentProjectId ) == ProjectStatus::Idle )
  {
    DeltaFileWrapper *currentDeltaFileWrapper = mLayerObserver->currentDeltaFileWrapper();

    mCanCommitCurrentProject = ( currentDeltaFileWrapper->count() > 0 );
  }
  else
  {
    mCanCommitCurrentProject = false;
  }

  if ( oldCanCommitCurrentProject != mCanCommitCurrentProject )
    emit canCommitCurrentProjectChanged();
}

bool QFieldCloudProjectsModel::canSyncCurrentProject()
{
  return mCanSyncCurrentProject;
}

void QFieldCloudProjectsModel::updateCanSyncCurrentProject()
{
  bool oldCanSyncCurrentProject = mCanSyncCurrentProject;

  if ( mCurrentProjectId.isEmpty() )
    mCanSyncCurrentProject = false;
  else if ( projectStatus( mCurrentProjectId ) == ProjectStatus::Idle
            && mLayerObserver->committedDeltaFileWrapper()->count() > 0 )
    mCanSyncCurrentProject = true;
  else if ( projectStatus( mCurrentProjectId ) == ProjectStatus::Idle
            && projectModification( mCurrentProjectId ) & RemoteModification )
    mCanSyncCurrentProject = true;
  else
    mCanSyncCurrentProject = false;

  if ( oldCanSyncCurrentProject != mCanSyncCurrentProject )
    emit canSyncCurrentProjectChanged();
}

QFieldCloudProjectsModel::ProjectModifications QFieldCloudProjectsModel::projectModification( const QString &projectId ) const
{
  const int index = findProject( projectId );

  if ( index == -1 || index >= mCloudProjects.size() )
    return NoModification;

  return mCloudProjects[index].modification;
}

void QFieldCloudProjectsModel::refreshProjectModification( const QString &projectId )
{
  const int index = findProject( projectId );

  if ( index == -1 || index >= mCloudProjects.size() )
    return;

  ProjectModifications oldModifications = mCloudProjects[index].modification;

  if ( mLayerObserver->currentDeltaFileWrapper()->count() > 0
       || mLayerObserver->committedDeltaFileWrapper()->count() > 0 )
    mCloudProjects[index].modification |= LocalModification;
  else
    mCloudProjects[index].modification ^= LocalModification;

  if ( oldModifications != mCloudProjects[index].modification )
  {
    QModelIndex idx = createIndex( index, 0 );
    emit dataChanged( idx, idx, QVector<int>() << ModificationRole );
  }
}

QString QFieldCloudProjectsModel::layerFileName( const QgsMapLayer *layer ) const
{
  return layer->dataProvider()->dataSourceUri().split( '|' )[0];
}

void QFieldCloudProjectsModel::downloadProject( const QString &projectId )
{
  if ( !mCloudConnection )
    return;

  int index = findProject( projectId );
  if ( index == -1 || index >= mCloudProjects.size() )
    return;

  // before downloading, the project should be idle
  if ( mCloudProjects[index].status != ProjectStatus::Idle )
    return;

  // before downloading, there should be no local modification, otherwise it will be overwritten
  if ( mCloudProjects[index].modification & LocalModification )
    return;

  mCloudProjects[index].downloadJobId = QString();
  mCloudProjects[index].downloadJobStatus = DownloadJobUnstartedStatus;
  mCloudProjects[index].downloadJobStatusString = QString();
  mCloudProjects[index].downloadFileTransfers.clear();
  mCloudProjects[index].downloadFilesFinished = 0;
  mCloudProjects[index].downloadFilesFailed = 0;
  mCloudProjects[index].downloadBytesTotal = 0;
  mCloudProjects[index].downloadBytesReceived = 0;
  mCloudProjects[index].downloadProgress = 0;

  mCloudProjects[index].status = ProjectStatus::Downloading;
  mCloudProjects[index].errorStatus = NoErrorStatus;
  mCloudProjects[index].modification = NoModification;
  QModelIndex idx = createIndex( index, 0 );
  emit dataChanged( idx, idx,  QVector<int>() << StatusRole << ErrorStatusRole << DownloadProgressRole );

  // //////////
  // 1) request a new export the project
  // //////////
  NetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/qfield-files/%1/" ).arg( projectId ) );

  connect( reply, &NetworkReply::finished, this, [ = ]()
  {
    QNetworkReply *rawReply = reply->reply();
    reply->deleteLater();

    if ( rawReply->error() != QNetworkReply::NoError )
    {
      mCloudProjects[index].status = ProjectStatus::Idle;

      QModelIndex idx = createIndex( index, 0 );
      emit dataChanged( idx, idx,  QVector<int>() << StatusRole << DownloadProgressRole );
      emit warning( QStringLiteral( "Error fetching project: %1" ).arg( rawReply->errorString() ) );

      return;
    }

    const QJsonObject downloadJson = QJsonDocument::fromJson( rawReply->readAll() ).object();
    mCloudProjects[index].downloadJobId = downloadJson.value( QStringLiteral( "jobid" ) ).toString();
    // TODO probably better idea to get the status from downloadJson body
    mCloudProjects[index].downloadJobStatus = DownloadJobPendingStatus;

    projectGetDownloadStatus( projectId );
  } );


  // //////////
  // 2) poll for download files export status until final status received
  // //////////
  connect( this, &QFieldCloudProjectsModel::networkDownloadStatusChecked, this, [ = ]( const QString & uploadedProjectId )
  {
    if ( projectId != uploadedProjectId )
      return;

    switch ( mCloudProjects[index].downloadJobStatus )
    {
      case DownloadJobUnstartedStatus:
        // download export job should be already started!!!
        Q_ASSERT( 0 );
        break;
      case DownloadJobPendingStatus:
      case DownloadJobQueuedStatus:
      case DownloadJobStartedStatus:
        // infinite retry, there should be one day, when we can get the status!
        QTimer::singleShot( sDelayBeforeStatusRetry, [ = ]()
        {
          projectGetDownloadStatus( projectId );
        } );
        break;
      case DownloadJobErrorStatus:
        emit downloadFinished( projectId, true, mCloudProjects[index].downloadJobStatusString );
        return;
      case DownloadJobCreatedStatus:
        // assumed that the file downloads are already in CloudProject.dowloadProjectFiles
        projectDownloadFiles( projectId );
        return;
    }
  } );
}


void QFieldCloudProjectsModel::projectGetDownloadStatus( const QString &projectId )
{
  const int index = findProject( projectId );

  if ( index == -1 || index >= mCloudProjects.size() )
    return;

  Q_ASSERT( index >= 0 && index < mCloudProjects.size() );
  Q_ASSERT( mCloudProjects[index].downloadJobStatus != DownloadJobUnstartedStatus );
  Q_ASSERT( ! mCloudProjects[index].downloadJobId.isEmpty() );

  QModelIndex idx = createIndex( index, 0 );
  NetworkReply *downloadStatusReply = mCloudConnection->get( QStringLiteral( "/api/v1/qfield-files/export/%1/" ).arg( mCloudProjects[index].downloadJobId ) );

  mCloudProjects[index].downloadJobStatusString = QString();

  connect( downloadStatusReply, &NetworkReply::finished, this, [ = ]()
  {
    QNetworkReply *rawReply = downloadStatusReply->reply();
    downloadStatusReply->deleteLater();

    Q_ASSERT( downloadStatusReply->isFinished() );
    Q_ASSERT( rawReply );

    if ( rawReply->error() != QNetworkReply::NoError )
    {
      int statusCode = rawReply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();

      mCloudProjects[index].status = ProjectStatus::Idle;
      mCloudProjects[index].errorStatus = DownloadErrorStatus;
      mCloudProjects[index].downloadJobStatus = DownloadJobErrorStatus;
      // TODO this is oversimplification. e.g. 404 error is when the requested export id is not existent
      mCloudProjects[index].downloadJobStatusString = QStringLiteral( "[HTTP%1] Networking error, please retry!" ).arg( statusCode );

      emit networkDownloadStatusChecked( projectId );

      return;
    }

    const QJsonObject payload = QJsonDocument::fromJson( rawReply->readAll() ).object();

    Q_ASSERT( ! payload.isEmpty() );

    const QString status = payload.value( QStringLiteral( "status" ) ).toString().toUpper();

    if ( status == QStringLiteral( "PENDING" ) )
      mCloudProjects[index].downloadJobStatus = DownloadJobPendingStatus;
    else if ( status == QStringLiteral( "QUEUED" ) )
      mCloudProjects[index].downloadJobStatus = DownloadJobQueuedStatus;
    else if ( status == QStringLiteral( "STARTED" ) )
      mCloudProjects[index].downloadJobStatus = DownloadJobStartedStatus;
    else if ( status == QStringLiteral( "FINISHED" ) )
    {
      mCloudProjects[index].downloadJobStatus = DownloadJobCreatedStatus;

      const QJsonArray files = payload.value( QStringLiteral( "files" ) ).toArray();
      for ( const QJsonValue file : files )
      {
        QJsonObject fileObject = file.toObject();
        QString fileName = fileObject.value( QStringLiteral( "name" ) ).toString();
        int fileSize = fileObject.value( QStringLiteral( "size" ) ).toInt();

        mCloudProjects[index].downloadFileTransfers.insert( fileName, FileTransfer( fileName, fileSize ) );
        mCloudProjects[index].downloadBytesTotal += fileSize;
      }
    }
    else if ( status == QStringLiteral( "ERROR" ) )
    {
      mCloudProjects[index].status = ProjectStatus::Idle;
      mCloudProjects[index].errorStatus = DownloadErrorStatus;
      mCloudProjects[index].downloadJobStatus = DownloadJobErrorStatus;
      mCloudProjects[index].downloadJobStatusString = payload.value( QStringLiteral( "output" ) ).toString().split( '\n' ).last();

      emit dataChanged( idx, idx, QVector<int>() << StatusRole << DownloadErrorStatus );
    }
    else
    {
      mCloudProjects[index].status = ProjectStatus::Idle;
      mCloudProjects[index].errorStatus = DownloadErrorStatus;
      mCloudProjects[index].downloadJobStatus = DownloadJobErrorStatus;
      mCloudProjects[index].downloadJobStatusString = QStringLiteral( "Unknown status \"%1\"" ).arg( status );

      emit dataChanged( idx, idx, QVector<int>() << StatusRole << DownloadErrorStatus );
    }

    emit networkDownloadStatusChecked( projectId );
  } );
}

void QFieldCloudProjectsModel::projectDownloadFiles( const QString &projectId )
{
  if ( !mCloudConnection )
    return;

  const int index = findProject( projectId );

  if ( index == -1 || index >= mCloudProjects.size() )
    return;

  const QStringList fileNames = mCloudProjects[index].downloadFileTransfers.keys();

  // why call download project files, if there are no project files?
  if ( fileNames.isEmpty() )
  {
    QModelIndex idx = createIndex( index, 0 );

    mCloudProjects[index].status = ProjectStatus::Idle;
    mCloudProjects[index].downloadProgress = 1;

    emit dataChanged( idx, idx,  QVector<int>() << StatusRole << DownloadProgressRole );
    emit warning( QStringLiteral( "Project empty" ) );

    return;
  }

  for ( const QString &fileName : fileNames )
  {
    NetworkReply *reply = downloadFile( mCloudProjects[index].downloadJobId, fileName );
    QTemporaryFile *file = new QTemporaryFile( reply );

    if ( ! file->open() )
    {
      QgsLogger::warning( QStringLiteral( "Failed to open temporary file for \"%1\", reason:\n%2" )
                          .arg( fileName )
                          .arg( file->errorString() ) );

      mCloudProjects[index].downloadFilesFailed++;

      emit projectDownloaded( projectId, true, mCloudProjects[index].name );

      return;
    }

    mCloudProjects[index].downloadFileTransfers[fileName].networkReply = reply;

    connect( reply, &NetworkReply::downloadProgress, this, [ = ]( int bytesReceived, int bytesTotal )
    {
      Q_UNUSED( bytesTotal );

      // it means the NetworkReply has failed and retried
      mCloudProjects[index].downloadBytesReceived -= mCloudProjects[index].downloadFileTransfers[fileName].bytesTransferred;
      mCloudProjects[index].downloadBytesReceived = bytesReceived;
      mCloudProjects[index].downloadProgress += static_cast<double>( mCloudProjects[index].downloadBytesTotal ) / mCloudProjects[index].downloadBytesReceived;

      QModelIndex idx = createIndex( index, 0 );

      emit dataChanged( idx, idx,  QVector<int>() << DownloadProgressRole );
    } );

    connect( reply, &NetworkReply::finished, this, [ = ]()
    {
      QVector<int> rolesChanged;
      QNetworkReply *rawReply = reply->reply();
      reply->deleteLater();

      Q_ASSERT( reply->isFinished() );
      Q_ASSERT( reply );

      mCloudProjects[index].downloadFilesFinished++;

      bool hasError = false;

      if ( ! hasError && rawReply->error() != QNetworkReply::NoError )
      {
        hasError = true;
        QgsLogger::warning( QStringLiteral( "Failed to download project file stored at \"%1\", reason:\n%2" ).arg( fileName, rawReply->errorString() ) );
      }

      if ( ! hasError && ! file->write( rawReply->readAll() ) )
      {
        hasError = true;
        QgsLogger::warning( QStringLiteral( "Failed to write downloaded file stored at \"%1\", reason:\n%2" ).arg( fileName ).arg( file->errorString() ) );
      }

      QFileInfo fileInfo( fileName );
      QDir dir( QStringLiteral( "%1/%2/%3" ).arg( QFieldCloudUtils::localCloudDirectory(), projectId, fileInfo.path() ) );

      if ( ! hasError && ! dir.exists() && ! dir.mkpath( QStringLiteral( "." ) ) )
      {
        hasError = true;
        QgsLogger::warning( QStringLiteral( "Failed to create directory at \"%1\"" ).arg( dir.path() ) );
      }

      const QString destinationFileName( dir.filePath( fileInfo.fileName() ) );

      // if the file already exists, we need to delete it first, as QT does not support overwriting
      // NOTE: it is possible that someone creates the file in the meantime between this and the next if statement
      if ( ! hasError && QFile::exists( destinationFileName ) && ! file->remove( destinationFileName ) )
      {
        hasError = true;
        QgsLogger::warning( QStringLiteral( "Failed to remove file before overwriting stored at \"%1\", reason:\n%2" ).arg( fileName ).arg( file->errorString() ) );
      }

      if ( ! hasError && ! file->copy( destinationFileName ) )
      {
        hasError = true;
        QgsLogger::warning( QStringLiteral( "Failed to write downloaded file stored at \"%1\", reason:\n%2" ).arg( fileName ).arg( file->errorString() ) );

        if ( ! QFile::remove( dir.filePath( fileName ) ) )
          QgsLogger::warning( QStringLiteral( "Failed to remove partly overwritten file stored at \"%1\"" ).arg( fileName ) );
      }

      if ( hasError )
      {
        mCloudProjects[index].downloadFilesFailed++;
        mCloudProjects[index].status = ProjectStatus::Idle;
        mCloudProjects[index].errorStatus = DownloadErrorStatus;

        emit projectDownloaded( projectId, true, mCloudProjects[index].name );
      }

      if ( mCloudProjects[index].downloadFilesFinished == fileNames.count() )
      {
        emit projectDownloaded( projectId, false, mCloudProjects[index].name );

        mCloudProjects[index].status = ProjectStatus::Idle;
        mCloudProjects[index].checkout = ProjectCheckout::LocalAndRemoteCheckout;
        mCloudProjects[index].localPath = QFieldCloudUtils::localProjectFilePath( projectId );

        rolesChanged << StatusRole << LocalPathRole << CheckoutRole;
      }

      QModelIndex idx = createIndex( index, 0 );

      emit dataChanged( idx, idx, rolesChanged );
    } );
  }
}


void QFieldCloudProjectsModel::uploadProject( const QString &projectId, const bool shouldDownloadUpdates )
{
  if ( !mCloudConnection )
    return;

  int index = findProject( projectId );

  if ( index == -1 )
    return;

  if ( !( mCloudProjects[index].status == ProjectStatus::Idle ) )
    return;

  if ( !( mCloudProjects[index].modification & LocalModification ) )
    return;

  if ( ! mLayerObserver->commit() )
  {
    QgsLogger::warning( QStringLiteral( "Failed to commit changes." ) );
    return;
  }

  QModelIndex idx = createIndex( index, 0 );
  DeltaFileWrapper *deltaFile = mLayerObserver->committedDeltaFileWrapper();

  if ( deltaFile->hasError() )
  {
    QgsLogger::warning( QStringLiteral( "The delta file has an error: %1" ).arg( deltaFile->errorString() ) );
    return;
  }

  mCloudProjects[index].status = ProjectStatus::Uploading;
  mCloudProjects[index].deltaFileId = deltaFile->id();
  mCloudProjects[index].deltaFileUploadStatus = DeltaFileLocalStatus;

  mCloudProjects[index].uploadAttachments.empty();
  mCloudProjects[index].uploadAttachmentsFinished = 0;
  mCloudProjects[index].uploadAttachmentsFailed = 0;

  refreshProjectModification( projectId );
  updateCanCommitCurrentProject();
  updateCanSyncCurrentProject();

  emit dataChanged( idx, idx,  QVector<int>() << StatusRole << UploadProgressRole );


  // //////////
  // prepare attachment files to be uploaded
  // //////////
  const QStringList attachmentFileNames = deltaFile->attachmentFileNames().keys();
  for ( const QString &fileName : attachmentFileNames )
  {
    const int fileSize = QFileInfo( fileName ).size();

    Q_ASSERT( ! fileName.isEmpty() );
//    Q_ASSERT( fileSize > 0 );

    // ? should we also check the checksums of the files being uploaded? they are available at deltaFile->attachmentFileNames()->values()

    mCloudProjects[index].uploadAttachments.insert( fileName, FileTransfer( fileName, fileSize ) );
    mCloudProjects[index].uploadAttachmentsBytesTotal += fileSize;
  }


  // //////////
  // 1) send delta file
  // //////////
  NetworkReply *deltasCloudReply = mCloudConnection->post(
                                     QStringLiteral( "/api/v1/deltas/%1/" ).arg( projectId ),
                                     QVariantMap(),
                                     QStringList( {deltaFile->fileName()} ) );

  Q_ASSERT( deltasCloudReply );

  connect( deltasCloudReply, &NetworkReply::finished, this, [ = ]()
  {
    QNetworkReply *deltasReply = deltasCloudReply->reply();
    deltasCloudReply->deleteLater();

    Q_ASSERT( deltasCloudReply->isFinished() );
    Q_ASSERT( deltasReply );

    // if there is an error, cannot continue sync
    if ( deltasReply->error() != QNetworkReply::NoError )
    {
      // TODO check why exactly we failed
      // maybe the project does not exist, then create it?
      QgsLogger::warning( QStringLiteral( "Failed to upload delta file, reason:\n%1" ).arg( deltasReply->errorString() ) );

      deltaFile->resetId();

      if ( ! deltaFile->toFile() )
        QgsLogger::warning( QStringLiteral( "Failed update committed delta file." ) );

      mCloudProjects[index].deltaFileUploadStatusString = deltasReply->errorString();

      projectCancelUpload( projectId );
      return;
    }

    mCloudProjects[index].deltaFileUploadStatus = DeltaFilePendingStatus;
    mCloudProjects[index].deltaLayersToDownload = deltaFile->deltaLayerIds();
    emit networkDeltaUploaded( projectId );
  } );


  // //////////
  // 2) delta successfully uploaded, then send attachments
  // //////////
  connect( this, &QFieldCloudProjectsModel::networkDeltaUploaded, this, [ = ]( const QString & uploadedProjectId )
  {
    if ( projectId != uploadedProjectId )
      return;

    // attachments can be uploaded in the background.
    // ? what if an attachment fail to be uploaded?
    projectUploadAttachments( projectId );

    if ( mCloudProjects[index].uploadAttachmentsFailed != 0 )
    {
      mCloudProjects[index].deltaFileUploadStatusString = QStringLiteral( "Some layers failed to download" );
      projectCancelUpload( projectId );
      return;
    }

    projectGetDeltaStatus( projectId );
  } );


  // //////////
  // 3) new delta status received. Never give up to get a successful status.
  // //////////
  connect( this, &QFieldCloudProjectsModel::networkDeltaStatusChecked, this, [ = ]( const QString & uploadedProjectId )
  {
    if ( projectId != uploadedProjectId )
      return;

    switch ( mCloudProjects[index].deltaFileUploadStatus )
    {
      case DeltaFileLocalStatus:
        // delta file should be already sent!!!
        Q_ASSERT( 0 );
        break;
      case DeltaFilePendingStatus:
      case DeltaFileWaitingStatus:
      case DeltaFileBusyStatus:
        // infinite retry, there should be one day, when we can get the status!
        QTimer::singleShot( sDelayBeforeStatusRetry, [ = ]()
        {
          projectGetDeltaStatus( projectId );
        } );
        break;
      case DeltaFileErrorStatus:
        deltaFile->resetId();

        if ( ! deltaFile->toFile() )
          QgsLogger::warning( QStringLiteral( "Failed update committed delta file." ) );

        projectCancelUpload( projectId );
        return;
      case DeltaFileAppliedStatus:
      case DeltaFileAppliedWithConflictsStatus:
        deltaFile->reset();
        deltaFile->resetId();

        if ( ! deltaFile->toFile() )
          QgsLogger::warning( QStringLiteral( "Failed to reset delta file. %1" ).arg( deltaFile->errorString() ) );

        mCloudProjects[index].modification ^= LocalModification;
        mCloudProjects[index].modification |= RemoteModification;

        emit dataChanged( idx, idx, QVector<int>() << ModificationRole );

        // download the updated files, so the files are for sure the same on the client and on the server
        if ( shouldDownloadUpdates )
          downloadProject( projectId );
        else
          emit syncFinished( projectId, false );
    }
  } );


// this code is no longer needed, as we do not upload or download files selectively
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//  // //////////
//  // 4) project downloaded, if all done, then reload the project and sync done!
//  // //////////
//  connect( this, &QFieldCloudProjectsModel::networkAllLayersDownloaded, this, [ = ]( const QString & callerProjectId )
//  {
//    if ( projectId != callerProjectId )
//      return;

//    // wait until all layers are downloaded
//    if ( mCloudProjects[index].downloadLayersFinished < mCloudProjects[index].deltaLayersToDownload.size() )
//      return;

//    // there are some files that failed to download
//    if ( mCloudProjects[index].downloadLayersFailed > 0 )
//    {
//      // TODO translate this message
//      mCloudProjects[index].deltaFileUploadStatusString = QStringLiteral( "Failed to retrieve some of the updated layers, but changes are committed on the server. "
//                            "Try to reload the project from the cloud." );
//      projectCancelUpload( projectId );

//      return;
//    }

//    QgsProject::instance()->reloadAllLayers();

//    emit dataChanged( idx, idx, QVector<int>() << StatusRole );
//    emit syncFinished( projectId, false );
//  } );
}


void QFieldCloudProjectsModel::projectGetDeltaStatus( const QString &projectId )
{
  const int index = findProject( projectId );

  Q_ASSERT( index >= 0 && index < mCloudProjects.size() );

  NetworkReply *deltaStatusReply = mCloudConnection->get( QStringLiteral( "/api/v1/deltas/%1/%2/" ).arg( mCloudProjects[index].id, mCloudProjects[index].deltaFileId ) );

  mCloudProjects[index].deltaFileUploadStatusString = QString();

  connect( deltaStatusReply, &NetworkReply::finished, this, [ = ]()
  {
    QNetworkReply *rawReply = deltaStatusReply->reply();
    deltaStatusReply->deleteLater();

    Q_ASSERT( deltaStatusReply->isFinished() );
    Q_ASSERT( rawReply );

    if ( rawReply->error() != QNetworkReply::NoError )
    {
      int statusCode = rawReply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();

      mCloudProjects[index].deltaFileUploadStatus = DeltaFileErrorStatus;
      // TODO this is oversimplification. e.g. 404 error is when the requested delta file id is not existant
      mCloudProjects[index].deltaFileUploadStatusString = QStringLiteral( "[HTTP%1] Networking error, please retry!" ).arg( statusCode );

      emit networkDeltaStatusChecked( projectId );

      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson( rawReply->readAll() );

    Q_ASSERT( doc.isObject() );

    const QString status = doc.object().value( QStringLiteral( "status" ) ).toString().toUpper();

    if ( status == QStringLiteral( "APPLIED" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileAppliedStatus;
    else if ( status == QStringLiteral( "APPLIED_WITH_CONFLICTS" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileAppliedWithConflictsStatus;
    else if ( status == QStringLiteral( "STATUS_PENDING" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFilePendingStatus;
    else if ( status == QStringLiteral( "STATUS_WAITING" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileWaitingStatus;
    else if ( status == QStringLiteral( "STATUS_BUSY" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileBusyStatus;
    else if ( status == QStringLiteral( "STATUS_ERROR" ) )
    {
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileErrorStatus;
      mCloudProjects[index].deltaFileUploadStatusString = doc.object().value( QStringLiteral( "output" ) ).toString().split( '\n' ).last();
    }
    else
    {
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileErrorStatus;
      mCloudProjects[index].deltaFileUploadStatusString = QStringLiteral( "Unknown status \"%1\"" ).arg( status );
      QgsLogger::warning( mCloudProjects[index].deltaFileUploadStatusString );
    }

    emit networkDeltaStatusChecked( projectId );
  } );
}

void QFieldCloudProjectsModel::projectUploadAttachments( const QString &projectId )
{
  const int index = findProject( projectId );
  QModelIndex idx = createIndex( index, 0 );

  Q_ASSERT( index >= 0 && index < mCloudProjects.size() );

  // start uploading the attachments
  const QStringList attachmentFileNames;
  for ( const QString &fileName : attachmentFileNames )
  {
    NetworkReply *attachmentCloudReply = uploadFile( mCloudProjects[index].id, fileName );

    mCloudProjects[index].uploadAttachments[fileName].networkReply = attachmentCloudReply;

    connect( attachmentCloudReply, &NetworkReply::uploadProgress, this, [ = ]( int bytesSent, int bytesTotal )
    {
      Q_UNUSED( bytesTotal );
      mCloudProjects[index].uploadAttachments[fileName].bytesTransferred = bytesSent;
      emit dataChanged( idx, idx, QVector<int>() << UploadProgressRole );
    } );

    connect( attachmentCloudReply, &NetworkReply::finished, this, [ = ]()
    {
      QNetworkReply *attachmentReply = attachmentCloudReply->reply();

      Q_ASSERT( attachmentCloudReply->isFinished() );
      Q_ASSERT( attachmentReply );

      mCloudProjects[index].uploadAttachmentsFinished++;

      emit dataChanged( idx, idx, QVector<int>() << UploadProgressRole );

      // if there is an error, don't panic, we continue uploading. The files may be later manually synced.
      if ( attachmentReply->error() != QNetworkReply::NoError )
      {
        mCloudProjects[index].uploadAttachmentsFailed++;
        QgsLogger::warning( QStringLiteral( "Failed to upload attachment stored at \"%1\", reason:\n%2" )
                            .arg( fileName )
                            .arg( attachmentReply->errorString() ) );
      }
    } );
  }
}

void QFieldCloudProjectsModel::projectCancelUpload( const QString &projectId )
{
  if ( ! mCloudConnection )
    return;

  int index = findProject( projectId );

  if ( index == -1 )
    return;

  const QStringList attachmentFileNames = mCloudProjects[index].uploadAttachments.keys();
  for ( const QString &attachmentFileName : attachmentFileNames )
  {
    NetworkReply *attachmentReply = mCloudProjects[index].uploadAttachments[attachmentFileName].networkReply;

    // the replies might be already disposed
    if ( ! attachmentReply )
      continue;

    // the replies might be already finished
    if ( attachmentReply->isFinished() )
      continue;

    attachmentReply->abort();
  }

  mCloudProjects[index].status = ProjectStatus::Idle;
  mCloudProjects[index].errorStatus = UploadErrorStatus;

  QModelIndex idx = createIndex( index, 0 );

  emit dataChanged( idx, idx, QVector<int>() << StatusRole << ErrorStatusRole );
  emit syncFinished( projectId, true, mCloudProjects[index].deltaFileUploadStatusString );

  return;
}

void QFieldCloudProjectsModel::connectionStatusChanged()
{
  refreshProjectsList();
}

void QFieldCloudProjectsModel::layerObserverLayerEdited( const QString &layerId )
{
  Q_UNUSED( layerId );

  const int index = findProject( mCurrentProjectId );

  if ( index == -1 || index >= mCloudProjects.size() )
  {
    QgsLogger::warning( QStringLiteral( "Layer observer triggered `isDirtyChanged` signal incorrectly" ) );
    return;
  }

  beginResetModel();

  const DeltaFileWrapper *committedDeltaFileWrapper = mLayerObserver->committedDeltaFileWrapper();

  Q_ASSERT( committedDeltaFileWrapper );

  if ( committedDeltaFileWrapper->count() > 0 )
    mCloudProjects[index].modification |= LocalModification;
  else
    mCloudProjects[index].modification ^= LocalModification;

  endResetModel();
}

void QFieldCloudProjectsModel::projectListReceived()
{
  NetworkReply *reply = qobject_cast<NetworkReply *>( sender() );
  QNetworkReply *rawReply = reply->reply();

  Q_ASSERT( rawReply );

  if ( rawReply->error() != QNetworkReply::NoError )
    return;

  QByteArray response = rawReply->readAll();

  QJsonDocument doc = QJsonDocument::fromJson( response );
  QJsonArray projects = doc.array();
  reload( projects );
}

NetworkReply *QFieldCloudProjectsModel::downloadFile( const QString &exportJobId, const QString &fileName )
{
  return mCloudConnection->get( QStringLiteral( "/api/v1/qfield-files/export/%1/%2/" ).arg( exportJobId, fileName ) );
}

NetworkReply *QFieldCloudProjectsModel::uploadFile( const QString &projectId, const QString &fileName )
{
  return mCloudConnection->post( QStringLiteral( "/api/v1/files/%1/%2/" ).arg( projectId, fileName ), QVariantMap(), QStringList( {fileName} ) );
}

QHash<int, QByteArray> QFieldCloudProjectsModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[IdRole] = "Id";
  roles[OwnerRole] = "Owner";
  roles[NameRole] = "Name";
  roles[DescriptionRole] = "Description";
  roles[ModificationRole] = "Modification";
  roles[CheckoutRole] = "Checkout";
  roles[StatusRole] = "Status";
  roles[ErrorStatusRole] = "ErrorStatus";
  roles[ErrorStringRole] = "ErrorString";
  roles[DownloadProgressRole] = "DownloadProgress";
  roles[LocalPathRole] = "LocalPath";
  return roles;
}

void QFieldCloudProjectsModel::reload( const QJsonArray &remoteProjects )
{
  beginResetModel();
  mCloudProjects.clear();

  for ( const auto project : remoteProjects )
  {
    QVariantHash projectDetails = project.toObject().toVariantHash();
    CloudProject cloudProject( projectDetails.value( "id" ).toString(),
                               projectDetails.value( "owner" ).toString(),
                               projectDetails.value( "name" ).toString(),
                               projectDetails.value( "description" ).toString(),
                               QString(),
                               RemoteCheckout,
                               ProjectStatus::Idle );

    const QString projectPrefix = QStringLiteral( "QFieldCloud/projects/%1" ).arg( cloudProject.id );
    QSettings().setValue( QStringLiteral( "%1/owner" ).arg( projectPrefix ), cloudProject.owner );
    QSettings().setValue( QStringLiteral( "%1/name" ).arg( projectPrefix ), cloudProject.name );
    QSettings().setValue( QStringLiteral( "%1/description" ).arg( projectPrefix ), cloudProject.description );
    QSettings().setValue( QStringLiteral( "%1/updatedAt" ).arg( projectPrefix ), cloudProject.updatedAt );

    QDir localPath( QStringLiteral( "%1/%2" ).arg( QFieldCloudUtils::localCloudDirectory(), cloudProject.id ) );
    if ( localPath.exists() )
    {
      cloudProject.checkout = LocalAndRemoteCheckout;
      cloudProject.localPath = QFieldCloudUtils::localProjectFilePath( cloudProject.id );
    }

    mCloudProjects << cloudProject;
  }

  QDirIterator projectDirs( QFieldCloudUtils::localCloudDirectory(), QDir::Dirs | QDir::NoDotAndDotDot );
  while ( projectDirs.hasNext() )
  {
    projectDirs.next();

    const QString projectId = projectDirs.fileName();
    int index = findProject( projectId );
    if ( index != -1 )
      continue;

    const QString projectPrefix = QStringLiteral( "QFieldCloud/projects/%1" ).arg( projectId );
    if ( !QSettings().contains( QStringLiteral( "%1/name" ).arg( projectPrefix ) ) )
      continue;

    const QString owner = QSettings().value( QStringLiteral( "%1/owner" ).arg( projectPrefix ) ).toString();
    const QString name = QSettings().value( QStringLiteral( "%1/name" ).arg( projectPrefix ) ).toString();
    const QString description = QSettings().value( QStringLiteral( "%1/description" ).arg( projectPrefix ) ).toString();
    const QString updatedAt = QSettings().value( QStringLiteral( "%1/updatedAt" ).arg( projectPrefix ) ).toString();

    CloudProject cloudProject( projectId, owner, name, description, QString(), LocalCheckout, ProjectStatus::Idle );
    cloudProject.localPath = QFieldCloudUtils::localProjectFilePath( cloudProject.id );
    mCloudProjects << cloudProject;

    Q_ASSERT( projectId == cloudProject.id );
  }

  endResetModel();
}

int QFieldCloudProjectsModel::rowCount( const QModelIndex &parent ) const
{
  if ( !parent.isValid() )
    return mCloudProjects.size();
  else
    return 0;
}

QVariant QFieldCloudProjectsModel::data( const QModelIndex &index, int role ) const
{
  if ( index.row() >= mCloudProjects.size() || index.row() < 0 )
    return QVariant();

  switch ( static_cast<ColumnRole>( role ) )
  {
    case IdRole:
      return mCloudProjects.at( index.row() ).id;
    case OwnerRole:
      return mCloudProjects.at( index.row() ).owner;
    case NameRole:
      return mCloudProjects.at( index.row() ).name;
    case DescriptionRole:
      return mCloudProjects.at( index.row() ).description;
    case ModificationRole:
      return static_cast<int>( mCloudProjects.at( index.row() ).modification );
    case CheckoutRole:
      return static_cast<int>( mCloudProjects.at( index.row() ).checkout );
    case StatusRole:
      return static_cast<int>( mCloudProjects.at( index.row() ).status );
    case ErrorStatusRole:
      return static_cast<int>( mCloudProjects.at( index.row() ).errorStatus );
    case ErrorStringRole:
      return mCloudProjects.at( index.row() ).errorStatus == DownloadErrorStatus
          ? mCloudProjects.at( index.row() ).downloadJobStatusString
          : mCloudProjects.at( index.row() ).errorStatus == UploadErrorStatus
            ? mCloudProjects.at( index.row() ).deltaFileUploadStatusString
            : QString();
    case DownloadProgressRole:
      return mCloudProjects.at( index.row() ).downloadProgress;
    case UploadProgressRole:
      // TODO
      return 1;
    case LocalPathRole:
      return mCloudProjects.at( index.row() ).localPath;
  }

  return QVariant();
}

bool QFieldCloudProjectsModel::discardLocalChangesFromCurrentProject()
{
  const int index = findProject( mCurrentProjectId );

  if ( index == -1 || index >= mCloudProjects.size() )
    return false;

  if ( ! mLayerObserver->commit() )
    return false;

  qDebug() << mLayerObserver->committedDeltaFileWrapper()->count() << mLayerObserver->currentDeltaFileWrapper()->count();

  DeltaFileWrapper *dfw = mLayerObserver->committedDeltaFileWrapper();

  if ( ! dfw->applyReversed() )
    return false;

  dfw->reset();
  dfw->resetId();

  if ( ! dfw->toFile() )
    return false;

  return true;
}
