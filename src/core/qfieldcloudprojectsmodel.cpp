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

  if ( layerObserver )
    connect( layerObserver, &LayerObserver::layerEdited, this, &QFieldCloudProjectsModel::layerObserverLayerEdited );

  mLayerObserver = layerObserver;

  emit layerObserverChanged();
}

QString QFieldCloudProjectsModel::currentCloudProjectId() const
{
  return mCurrentCloudProjectId;
}

void QFieldCloudProjectsModel::setCurrentCloudProjectId( const QString &currentCloudProjectId )
{
  if ( mCurrentCloudProjectId == currentCloudProjectId )
    return;

  mCurrentCloudProjectId = currentCloudProjectId;
  emit currentCloudProjectIdChanged();
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
      if ( mCloudProjects.at( index ).status == ProjectStatus::Idle && mCloudProjects.at( index ).checkout & LocalCheckout )
      {
        mCloudProjects[index].localPath = QString();
        QModelIndex idx = createIndex( index, 0 );
        emit dataChanged( idx, idx,  QVector<int>() << StatusRole << LocalPathRole );
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
    return QFieldCloudProjectsModel::ProjectStatus::Error;

  return mCloudProjects[index].status;
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

  // TODO
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
  if ( index > -1 )
  {
    mCloudProjects[index].downloadProjectFiles.clear();
    mCloudProjects[index].downloadProjectFilesFinished = 0;
    mCloudProjects[index].downloadProjectFilesFailed = 0;
    mCloudProjects[index].downloadProjectFilesBytesTotal = 0;
    mCloudProjects[index].downloadProjectFilesBytesReceived = 0;
    mCloudProjects[index].downloadProjectFilesProgress = 0;

    mCloudProjects[index].checkout = LocalFromRemoteCheckout;
    mCloudProjects[index].status = ProjectStatus::Downloading;
    mCloudProjects[index].modification = NoModification;
    QModelIndex idx = createIndex( index, 0 );
    emit dataChanged( idx, idx,  QVector<int>() << StatusRole << DownloadProgressRole );
  }

  NetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/files/%1/" ).arg( projectId ) );

  connect( reply, &NetworkReply::finished, this, [ = ]()
  {
    QNetworkReply *rawReply = reply->reply();
    reply->deleteLater();

    if ( rawReply->error() != QNetworkReply::NoError )
    {
      if ( index > -1 )
        mCloudProjects[index].status = ProjectStatus::Idle;

      emit warning( QStringLiteral( "Error fetching project: %1" ).arg( rawReply->errorString() ) );

      return;
    }

    const QJsonArray files = QJsonDocument::fromJson( rawReply->readAll() ).array();
    for ( const QJsonValue file : files )
    {
      QJsonObject fileObject = file.toObject();
      QString fileName = fileObject.value( QStringLiteral( "name" ) ).toString();
      int fileSize = fileObject.value( QStringLiteral( "size" ) ).toInt();

      mCloudProjects[index].downloadProjectFiles.insert( fileName, FileTransfer( fileName, fileSize ) );
      mCloudProjects[index].downloadProjectFilesBytesTotal += fileSize;
    }

    // download the files and if all good emit projectDownloaded
    projectDownloadFiles( projectId );
  } );
}


void QFieldCloudProjectsModel::projectDownloadFiles( const QString &projectId )
{
  if ( !mCloudConnection )
    return;

  int index = findProject( projectId );

  if ( index == -1 )
    return;

  const QStringList fileNames = mCloudProjects[index].downloadProjectFiles.keys();

  // why call download project files, if there are no project files?
  Q_ASSERT( fileNames.size() > 0 );

  for ( const QString &fileName : fileNames )
  {
    NetworkReply *reply = downloadFile( projectId, fileName );
    QTemporaryFile *file = new QTemporaryFile( reply );

    if ( ! file->open() )
    {
      QgsLogger::warning( QStringLiteral( "Failed to open temporary file for \"%1\", reason:\n%2" )
                          .arg( fileName )
                          .arg( file->errorString() ) );

      mCloudProjects[index].downloadProjectFilesFailed++;

      emit projectDownloaded( projectId, mCloudProjects[index].name, true );

      return;
    }

    mCloudProjects[index].downloadProjectFiles[fileName].networkReply = reply;

    connect( reply, &NetworkReply::downloadProgress, this, [ = ]( int bytesReceived, int bytesTotal )
    {
      Q_UNUSED( bytesTotal );

      // it means the NetworkReply has failed and retried
      mCloudProjects[index].downloadProjectFilesBytesReceived -= mCloudProjects[index].downloadProjectFiles[fileName].bytesTransferred;
      mCloudProjects[index].downloadProjectFilesBytesReceived = bytesReceived;
      mCloudProjects[index].downloadProjectFilesProgress += static_cast<double>( mCloudProjects[index].downloadProjectFilesBytesTotal ) / mCloudProjects[index].downloadProjectFilesBytesReceived;

      QVector<int> rolesChanged( {DownloadProgressRole} );
      QModelIndex idx = createIndex( index, 0 );

      emit dataChanged( idx, idx, rolesChanged );
    } );

    connect( reply, &NetworkReply::finished, this, [ = ]()
    {
      QVector<int> rolesChanged;
      QNetworkReply *rawReply = reply->reply();
      reply->deleteLater();

      Q_ASSERT( reply->isFinished() );
      Q_ASSERT( reply );

      mCloudProjects[index].downloadProjectFilesFinished++;

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
        mCloudProjects[index].downloadProjectFilesFailed++;
        mCloudProjects[index].status = ProjectStatus::Error;

        emit projectDownloaded( projectId, mCloudProjects[index].name, true );
      }

      if ( mCloudProjects[index].downloadProjectFilesFinished == fileNames.count() )
      {
        emit projectDownloaded( projectId, mCloudProjects[index].name, false );

        mCloudProjects[index].status = ProjectStatus::Idle;
        mCloudProjects[index].localPath = QFieldCloudUtils::localProjectFilePath( projectId );

        rolesChanged << StatusRole << LocalPathRole;
      }

      QModelIndex idx = createIndex( index, 0 );

      emit dataChanged( idx, idx, rolesChanged );
    } );
  }
}


void QFieldCloudProjectsModel::uploadProject( const QString &projectId )
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

  QModelIndex idx = createIndex( index, 0 );
  emit dataChanged( idx, idx,  QVector<int>() << StatusRole << UploadProgressRole );

  const DeltaFileWrapper *deltaFile = mLayerObserver->committedDeltaFileWrapper();

  if ( deltaFile->hasError() )
  {
    QgsLogger::warning( QStringLiteral( "The delta file has an error: %1" ).arg( deltaFile->errorString() ) );
    return;
  }

  mCloudProjects[index].status = ProjectStatus::Uploading;
  mCloudProjects[index].deltaFileId = deltaFile->id();
  mCloudProjects[index].deltaFileUploadStatus = DeltaFileLocalStatus;

  mCloudProjects[index].uploadOfflineLayers.empty();
  mCloudProjects[index].uploadOfflineLayersFinished = 0;
  mCloudProjects[index].uploadOfflineLayersFailed = 0;
  mCloudProjects[index].uploadOfflineLayersBytesTotal = 0;

  mCloudProjects[index].uploadAttachments.empty();
  mCloudProjects[index].uploadAttachmentsFinished = 0;
  mCloudProjects[index].uploadAttachmentsFailed = 0;


  // //////////
  // prepare offline layer files to be uploaded
  // //////////
  const QStringList offlineLayerIds = deltaFile->offlineLayerIds();
  for ( const QString &layerId : offlineLayerIds )
  {
    const QgsVectorLayer *vl = static_cast<QgsVectorLayer *>( QgsProject::instance()->mapLayer( layerId ) );

    Q_ASSERT( vl );

    const QString fileName = layerFileName( vl );
    const int fileSize = QFileInfo( fileName ).size();

    Q_ASSERT( ! fileName.isEmpty() );
    Q_ASSERT( fileSize > 0 );

    // TODO make sure that the layers are not editable and the user cannot interact with the project!!! Otherwise, we can upload half-commited data!!!

    // there might be multiple layers comming out of a single file, but we need to upload layer only once
    if ( ! mCloudProjects[index].uploadOfflineLayers.contains( fileName ) )
      mCloudProjects[index].uploadOfflineLayers.insert( fileName, FileTransfer( fileName, fileSize ) );

    mCloudProjects[index].uploadOfflineLayers[fileName].layerIds.append( layerId );
    mCloudProjects[index].uploadOfflineLayersBytesTotal += fileSize;
  }


  // //////////
  // prepare attachment files to be uploaded
  // //////////
  const QStringList attachmentFileNames = deltaFile->attachmentFileNames().keys();
  for ( const QString &fileName : attachmentFileNames )
  {
    const int fileSize = QFileInfo( fileName ).size();

    Q_ASSERT( ! fileName.isEmpty() );
    Q_ASSERT( fileSize > 0 );

    // ? should we also check the checksums of the files being uploaded? they are available at deltaFile->attachmentFileNames()->values()

    mCloudProjects[index].uploadAttachments.insert( fileName, FileTransfer( fileName, fileSize ) );
    mCloudProjects[index].uploadAttachmentsBytesTotal += fileSize;
  }


  // //////////
  // 1) send delta file
  // //////////
  NetworkReply *deltasCloudReply = mCloudConnection->post(
                                     QStringLiteral( "/api/v1/deltas/%1/" ).arg( projectId ),
                                     QVariantMap(
  {
    {"data", deltaFile->toJson()}
  } ) );

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

      projectCancelUpload( projectId, false );

      return;
    }

    mCloudProjects[index].deltaFileUploadStatus = DeltaFilePendingStatus;
    emit networkDeltaUploaded( projectId );
  } );


  // //////////
  // 2) delta successfully uploaded, then send offline layers
  // //////////
  connect( this, &QFieldCloudProjectsModel::networkDeltaUploaded, this, [ = ]( const QString & uploadedProjectId )
  {
    if ( projectId != uploadedProjectId )
      return;

    // offline layers should be uploaded before we continue with the next step
    // if there are no offline layers to be uploaded, just emit success and continue
    projectUploadOfflineLayers( projectId );

    // attachments can be uploaded in the background.
    // ? what if an attachment fail to be uploaded?
    projectUploadAttachments( projectId );
  } );


  // //////////
  // 3) offline layers successfully sent, then check delta status
  // //////////
  connect( this, &QFieldCloudProjectsModel::networkAllOfflineLayersUploaded, this, [this, projectId, index]( const QString & uploadedProjectId )
  {
    if ( projectId != uploadedProjectId )
      return;

    Q_ASSERT( mCloudProjects[index].layersDownloadedFinished == mCloudProjects[index].uploadOfflineLayers.size() );
    Q_ASSERT( mCloudProjects[index].layersDownloadedFailed == 0 );

    projectGetDeltaStatus( projectId );
  } );


  // //////////
  // 4) new delta status received. Never give up to get a successful status.
  // //////////
  connect( this, &QFieldCloudProjectsModel::networkDeltaStatusChecked, this, [this, projectId, index]( const QString & uploadedProjectId )
  {
    if ( projectId != uploadedProjectId )
      return;

    switch ( mCloudProjects[index].deltaFileUploadStatus )
    {
      case DeltaFileLocalStatus:
        // delta file should be already sent!!!
        Q_ASSERT( 0 );
        break;
      case DeltaFileErrorStatus:
      case DeltaFilePendingStatus:
      case DeltaFileWaitingStatus:
      case DeltaFileBusyStatus:
        // infinite retry, there should be one day, when we can get the status!
        QTimer::singleShot( sDelayBeforeDeltaStatusRetry, this, [ = ]()
        {
          projectGetDeltaStatus( projectId );
        } );
        break;
      case DeltaFileAppliedStatus:
      case DeltaFileAppliedWithConflictsStatus:
        projectDownloadLayers( projectId );
        break;
      default:
        Q_ASSERT( 0 );
    }

    projectGetDeltaStatus( projectId );
  } );


  // //////////
  // 5) layer downloaded, if all done, then reload the project and sync done!
  // //////////
  connect( this, &QFieldCloudProjectsModel::networkLayerDownloaded, this, [this, projectId, index]( const QString & callerProjectId )
  {
    if ( projectId != callerProjectId )
      return;

    // wait until all layers are downloaded
    if ( mCloudProjects[index].layersDownloadedFinished < mCloudProjects[index].deltaLayersToDownload.size() )
      return;

    // there are some files that failed to download
    if ( mCloudProjects[index].layersDownloadedFailed > 0 )
    {
      mCloudProjects[index].status = ProjectStatus::Error;
      // TODO translate this message
      const QString reason( "Failed to retrieve some of the updated layers, but changes are commited on the server. "
                            "Try to reload the project from the cloud." );
      emit syncFailed( projectId, reason );
      return;
    }

    mCloudProjects[index].status = ProjectStatus::Idle;
    QgsProject::instance()->reloadAllLayers();
  } );
}

void QFieldCloudProjectsModel::projectDownloadLayers( const QString &projectId )
{
  const int index = findProject( projectId );

  Q_ASSERT( index >= 0 && index < mCloudProjects.size() );

  const QStringList layerFileNames = mCloudProjects[index].deltaLayersToDownload;

  // there might be no layers to download
  if ( layerFileNames.isEmpty() )
  {
    emit networkAllLayersDownloaded( projectId );
    return;
  }

  for ( const QString &layerFileName : layerFileNames )
  {
    NetworkReply *reply = downloadFile( mCloudProjects[index].id, layerFileName );
    QTemporaryFile *file = new QTemporaryFile( reply );

    Q_ASSERT( file->open() );

    mCloudProjects[index].downloadLayers[layerFileName] = FileTransfer( layerFileName, 0, reply );

    connect( reply, &NetworkReply::downloadProgress, this, [ = ]( int bytesReceived, int bytesTotal )
    {
      Q_UNUSED( bytesTotal );
      mCloudProjects[index].uploadOfflineLayers[layerFileName].bytesTransferred = bytesReceived;
    } );

    connect( reply, &NetworkReply::finished, this, [ = ]()
    {
      QNetworkReply *rawReply = reply->reply();
      reply->deleteLater();

      Q_ASSERT( reply->isFinished() );
      Q_ASSERT( reply );

      mCloudProjects[index].downloadLayersFinished++;

      if ( rawReply->error() != QNetworkReply::NoError )
      {
        QgsLogger::warning( QStringLiteral( "Failed to download layer stored at \"%1\", reason:\n%2" )
                            .arg( layerFileName )
                            .arg( rawReply->errorString() ) );

        mCloudProjects[index].uploadOfflineLayersFailed++;
        projectCancelUpload( mCloudProjects[index].id, true );

        return;
      }

      if ( ! file->write( rawReply->readAll() ) )
      {
        QgsLogger::warning( QStringLiteral( "Failed to write downloaded layer stored at \"%1\", reason:\n%2" )
                            .arg( layerFileName )
                            .arg( rawReply->errorString() ) );

        return;
      }

      emit networkLayerDownloaded( projectId );

      if ( mCloudProjects[index].downloadLayersFinished == layerFileNames.count() )
        emit networkAllLayersDownloaded( projectId );

    } );
  }
}

void QFieldCloudProjectsModel::projectGetDeltaStatus( const QString &projectId )
{
  const int index = findProject( projectId );

  Q_ASSERT( index >= 0 && index < mCloudProjects.size() );

  NetworkReply *deltaStatusReply = mCloudConnection->get( QStringLiteral( "/api/v1/deltas/%1/status" ).arg( mCloudProjects[index].deltaFileId ) );

  connect( deltaStatusReply, &NetworkReply::finished, this, [this, index, projectId, deltaStatusReply]()
  {
    QNetworkReply *rawReply = deltaStatusReply->reply();
    deltaStatusReply->deleteLater();

    Q_ASSERT( deltaStatusReply->isFinished() );
    Q_ASSERT( rawReply );

    if ( rawReply->error() != QNetworkReply::NoError )
    {
      // never give up to get the status
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileErrorStatus;
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
    else if ( status == QStringLiteral( "PENDING" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFilePendingStatus;
    else if ( status == QStringLiteral( "WAITING" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileWaitingStatus;
    else if ( status == QStringLiteral( "Busy" ) )
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileBusyStatus;
    else
    {
      QgsLogger::warning( QStringLiteral( "Unknown status \"%1\"" ).arg( status ) );
      mCloudProjects[index].deltaFileUploadStatus = DeltaFileErrorStatus;
      Q_ASSERT( 0 );
    }
  } );
}

void QFieldCloudProjectsModel::projectUploadOfflineLayers( const QString &projectId )
{
  const int index = findProject( projectId );

  Q_ASSERT( index >= 0 && index < mCloudProjects.size() );
  Q_ASSERT( mCloudProjects[index].uploadOfflineLayers.size() > 0 );

  if ( mCloudProjects[index].uploadOfflineLayers.size() == 0 )
  {
    emit networkAllOfflineLayersUploaded( projectId );
    return;
  }

  const QStringList offlineLayerFileNames = mCloudProjects[index].uploadOfflineLayers.keys();

  for ( const QString &offlineLayerFileName : offlineLayerFileNames )
  {
    NetworkReply *offlineLayerCloudReply = uploadFile( mCloudProjects[index].id, offlineLayerFileName );

    mCloudProjects[index].uploadOfflineLayers[offlineLayerFileName].networkReply = offlineLayerCloudReply;

    connect( offlineLayerCloudReply, &NetworkReply::uploadProgress, this, [this, index, offlineLayerFileName]( int bytesSent, int bytesTotal )
    {
      Q_UNUSED( bytesTotal );
      mCloudProjects[index].uploadOfflineLayers[offlineLayerFileName].bytesTransferred = bytesSent;
    } );

    connect( offlineLayerCloudReply, &NetworkReply::finished, this, [this, index, projectId, offlineLayerCloudReply, offlineLayerFileName]()
    {
      QNetworkReply *offlineLayerReply = offlineLayerCloudReply->reply();
      offlineLayerCloudReply->deleteLater();

      Q_ASSERT( offlineLayerCloudReply->isFinished() );
      Q_ASSERT( offlineLayerReply );

      mCloudProjects[index].uploadOfflineLayersFinished++;

      // if offline layer upload fails, the whole transaction should be considered as failed
      if ( offlineLayerReply->error() != QNetworkReply::NoError )
      {
        QgsLogger::warning( QStringLiteral( "Failed to upload offline layer stored at \"%1\", reason:\n%2" )
                            .arg( offlineLayerFileName )
                            .arg( offlineLayerReply->errorString() ) );

        mCloudProjects[index].uploadOfflineLayersFailed++;
        projectCancelUpload( mCloudProjects[index].id, true );

        return;
      }

      emit networkOfflineLayerUploaded( projectId );

      if ( mCloudProjects[index].uploadOfflineLayersFinished == mCloudProjects[index].uploadOfflineLayers.size() )
        emit networkAllOfflineLayersUploaded( projectId );
    } );
  }
}

void QFieldCloudProjectsModel::projectUploadAttachments( const QString &projectId )
{
  const int index = findProject( projectId );

  Q_ASSERT( index >= 0 && index < mCloudProjects.size() );

  // start uploading the attachments
  const QStringList attachmentFileNames;
  for ( const QString &fileName : attachmentFileNames )
  {
    NetworkReply *attachmentCloudReply = uploadFile( mCloudProjects[index].id, fileName );

    mCloudProjects[index].uploadAttachments[fileName].networkReply = attachmentCloudReply;

    connect( attachmentCloudReply, &NetworkReply::uploadProgress, this, [this, index, fileName]( int bytesSent, int bytesTotal )
    {
      Q_UNUSED( bytesTotal );
      mCloudProjects[index].uploadAttachments[fileName].bytesTransferred = bytesSent;
    } );

    connect( attachmentCloudReply, &NetworkReply::finished, this, [this, index, fileName, attachmentCloudReply]()
    {
      QNetworkReply *attachmentReply = attachmentCloudReply->reply();

      Q_ASSERT( attachmentCloudReply->isFinished() );
      Q_ASSERT( attachmentReply );

      mCloudProjects[index].uploadAttachmentsFinished++;

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

void QFieldCloudProjectsModel::projectCancelUpload( const QString &projectId, bool shouldCancelAtServer )
{
  if ( ! mCloudConnection )
    return;

  int index = findProject( projectId );

  if ( index == -1 )
    return;

  mCloudProjects[index].status = ProjectStatus::Idle;

  const QStringList offlineLayerNames = mCloudProjects[index].uploadOfflineLayers.keys();
  for ( const QString &offlineLayerFileName : offlineLayerNames )
  {
    NetworkReply *offlineLayerReply = mCloudProjects[index].uploadOfflineLayers[offlineLayerFileName].networkReply;

    // the replies might be already disposed
    if ( ! offlineLayerReply )
      continue;

    // the replies might be already finished
    if ( offlineLayerReply->isFinished() )
      continue;

    offlineLayerReply->abort();
  }

  const QStringList attachmentFileNames = mCloudProjects[index].uploadOfflineLayers.keys();
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

  if ( shouldCancelAtServer )
  {
    // NetworkReply &reply = CloudReply::deleteResource( QStringLiteral( "/api/v1/deltas/%1" ) );
  }

  return;
}

void QFieldCloudProjectsModel::connectionStatusChanged()
{
  refreshProjectsList();
}

void QFieldCloudProjectsModel::layerObserverLayerEdited( const QString &layerId )
{
  Q_UNUSED( layerId );

  const int index = findProject( mCurrentCloudProjectId );

  if ( index == -1 || index >= mCloudProjects.size() )
  {
    QgsLogger::warning( QStringLiteral( "Layer observer triggered `isDirtyChanged` signal incorrectly" ) );
    return;
  }

  beginResetModel();

  const DeltaFileWrapper *committedDeltaFileWrapper = mLayerObserver->committedDeltaFileWrapper();

  Q_ASSERT( committedDeltaFileWrapper );

  if ( committedDeltaFileWrapper->count() > 0 || committedDeltaFileWrapper->offlineLayerIds().size() > 0 )
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

NetworkReply *QFieldCloudProjectsModel::downloadFile( const QString &projectId, const QString &fileName )
{
  return mCloudConnection->get( QStringLiteral( "/api/v1/files/%1/%2/" ).arg( projectId, fileName ) );
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
      cloudProject.checkout = LocalFromRemoteCheckout;
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
    case DownloadProgressRole:
      return mCloudProjects.at( index.row() ).downloadProjectFilesProgress;
    case UploadProgressRole:
      // TODO
      return 1;
    case LocalPathRole:
      return mCloudProjects.at( index.row() ).localPath;
  }

  return QVariant();
}

