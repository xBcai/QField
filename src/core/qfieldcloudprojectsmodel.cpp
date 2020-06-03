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

void QFieldCloudProjectsModel::setCurrentCloudProjectId(const QString &currentCloudProjectId)
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
      CloudReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/projects/" ) );
      connect( reply, &CloudReply::finished, this, &QFieldCloudProjectsModel::projectListReceived );
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
  for( int i = 0; i < cloudProjects.count(); i++ )
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

void QFieldCloudProjectsModel::downloadProject( const QString &projectId )
{
  if ( !mCloudConnection )
    return;

  int index = findProject( projectId );
  if ( index > -1 )
  {
    mCloudProjects[index].files.clear();
    mCloudProjects[index].filesSize = 0;
    mCloudProjects[index].filesFailed = 0;
    mCloudProjects[index].downloadedSize = 0;
    mCloudProjects[index].downloadProgress = 0.0;
    mCloudProjects[index].checkout = ProjectCheckout::LocalFromRemote;
    mCloudProjects[index].status = ProjectStatus::Downloading;
    mCloudProjects[index].modification = NoModification;
    QModelIndex idx = createIndex( index, 0 );
    emit dataChanged( idx, idx,  QVector<int>() << StatusRole << DownloadProgressRole );
  }

  QfNetworkReply *filesReply = mCloudConnection->get( QStringLiteral( "/api/v1/files/%1/" ).arg( projectId ) );

  connect( filesReply, &QfNetworkReply::finished, this, [this, index, projectId, filesReply]()
  {
    if ( filesReply->reply()->error() == QNetworkReply::NoError )
    {
      const QJsonArray files = QJsonDocument::fromJson( filesReply->reply()->readAll() ).array();
      for ( const auto file : files )
      {
        QJsonObject fileObject = file.toObject();
        QString fileName = fileObject.value( QStringLiteral( "name" ) ).toString();
        int fileSize = fileObject.value( QStringLiteral( "size" ) ).toInt();

        mCloudProjects[index].files.insert( fileName, fileSize );
        mCloudProjects[index].filesSize += fileSize;

        downloadFile( projectId, fileName );
      }

      if ( files.size() == 0 )
      {
        if ( index > -1 )
        {
          mCloudProjects[index].status = ProjectStatus::Idle;
          QModelIndex idx = createIndex( index, 0 );
          emit dataChanged( idx, idx,  QVector<int>() << StatusRole << DownloadProgressRole );
        }
        emit warning( QStringLiteral( "Project empty" ) );
      }
    }
    else
    {
      if ( index > -1 )
      {
        mCloudProjects[index].status = ProjectStatus::Idle;
        QModelIndex idx = createIndex( index, 0 );
        emit dataChanged( idx, idx,  QVector<int>() << StatusRole << DownloadProgressRole );
      }
      emit warning( QStringLiteral( "Error fetching project: %1" ).arg( filesReply->reply()->errorString() ) );
    }

    filesReply->deleteLater();
  } );
}


void QFieldCloudProjectsModel::uploadProject( const QString &projectId )
{
  if ( !mCloudConnection )
    return;

  int index = findProject( projectId );

  if ( index == -1 )
    return;

  if ( ! ( mCloudProjects[index].status == ProjectStatus::Idle ) )
    return;

  if ( ! ( mCloudProjects[index].modification & ProjectModification::Local ) )
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

  mCloudProjects[index].offlineLayers.empty();
  mCloudProjects[index].offlineLayersFinished = 0;
  mCloudProjects[index].offlineLayersFailed = 0;

  mCloudProjects[index].attachments.empty();
  mCloudProjects[index].attachmentsFinished = 0;
  mCloudProjects[index].attachmentsFailed = 0;


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

    mCloudProjects[index].offlineLayers.insert( fileName, FileTransfer( fileName, fileSize ) );
    mCloudProjects[index].offlineLayers[fileName].layerIds.append( layerId );
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

    mCloudProjects[index].attachments.insert( fileName, FileTransfer( fileName, fileSize ) );
  }

  // //////////
  // send delta file
  // //////////
  QfNetworkReply *deltasCloudReply = mCloudConnection->post( QStringLiteral( "/api/v1/deltas/%1/" ).arg( projectId ), QVariantMap({
    {"data", deltaFile->toJson()}
  }) );

  Q_ASSERT( deltasCloudReply );

  connect( deltasCloudReply, &QfNetworkReply::finished, this, [this, index, projectId, deltasCloudReply]()
  {
    QNetworkReply *deltasReply = deltasCloudReply->reply();

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

    projectUploadOfflineLayers( index );

    QfNetworkReply *deltaStatusReply = mCloudConnection->get( QStringLiteral( "/api/v1/deltas/%1/status" ).arg( mCloudProjects[index].deltaFileId ) );




//    connect( deltaStatusReply, &QfNetworkReply::finished, this, [this, index, deltaStatusReply]()
//    {
//      QNetworkReply *rawReply = deltaStatusReply->reply();

//      Q_ASSERT( deltaStatusReply->isFinished() );
//      Q_ASSERT( rawReply );

//      if ( rawReply->error() != QNetworkReply::NoError )
//      {
//        projectGetDeltaStatus( mCloudProjects[index].id );
//        return;
//      }

//      const QJsonDocument doc = QJsonDocument::fromJson( rawReply->readAll() );

//      Q_ASSERT( doc.isObject() );

//      const QString status = doc.object().value( QStringLiteral( "status" ) ).toString().toUpper();


//      if ( status == QStringLiteral( "APPLIED" ) )
//      {

//      }
//      else if ( status == QStringLiteral( "APPLIED_WITH_CONFLICTS" ) )
//      {

//      }
//      else if ( status == QStringLiteral( "PENDING" ) )
//      {

//      }
//      else if ( status == QStringLiteral( "SYNCING" ) )
//      {
//        QTimer::singleShot( 1000, this, []()
//        {
//          projectGetDeltaStatus( mCloudProjects[index].id );
//        });
//      }
//      else
//      {
//        QgsLogger::warning( QStringLiteral( "Unknown status \"%1\"" ).arg( status ) );
//        Q_ASSERT( 0 );
//      }
//    });

    projectUploadAttachments( index );

    // TODO download synced files



    // TODO reload the
    // reloads only the data layers
    // Qgs::instance()->reloadAllLayers();

  } );
}

void QFieldCloudProjectsModel::projectUploadOfflineLayers( const int index )
{
  const QString projectId = mCloudProjects[index].id;
  const QStringList offlineLayerFileNames = mCloudProjects[index].offlineLayers.keys();

  for ( const QString &offlineLayerFileName : offlineLayerFileNames )
  {
    QfNetworkReply *offlineLayerCloudReply = uploadFile( mCloudProjects[index].id, offlineLayerFileName );

    mCloudProjects[index].offlineLayers[offlineLayerFileName].networkReply = offlineLayerCloudReply;

    connect( offlineLayerCloudReply, &QfNetworkReply::uploadProgress, this, [this, index, offlineLayerFileName](int bytesSent, int bytesTotal)
    {
      Q_UNUSED( bytesTotal );
      mCloudProjects[index].offlineLayers[offlineLayerFileName].bytesTransferred = bytesSent;
    });

    connect( offlineLayerCloudReply, &QfNetworkReply::finished, this, [this, index, offlineLayerCloudReply, offlineLayerFileName]()
    {
      QNetworkReply *offlineLayerReply = offlineLayerCloudReply->reply();

      Q_ASSERT( offlineLayerCloudReply->isFinished() );
      Q_ASSERT( offlineLayerReply );

      mCloudProjects[index].offlineLayersFinished++;

      // if offline layer upload fails, the whole transaction should be considered as failed
      if ( offlineLayerReply->error() != QNetworkReply::NoError )
      {
        QgsLogger::warning( QStringLiteral( "Failed to upload offline layer stored at \"%1\", reason:\n%2" )
                            .arg( offlineLayerFileName )
                            .arg( offlineLayerReply->errorString() ) );

        mCloudProjects[index].offlineLayersFailed++;
        projectCancelUpload( mCloudProjects[index].id, true );

        return;
      }
    } );
  }
}

void QFieldCloudProjectsModel::projectUploadAttachments( const int index )
{
  // start uploading the attachments
  const QStringList attachmentFileNames;
  for ( const QString &fileName : attachmentFileNames )
  {
    QfNetworkReply *attachmentCloudReply = uploadFile( mCloudProjects[index].id, fileName );

    mCloudProjects[index].attachments[fileName].networkReply = attachmentCloudReply;

    connect( attachmentCloudReply, &QfNetworkReply::uploadProgress, this, [this, index, fileName](int bytesSent, int bytesTotal)
    {
      Q_UNUSED( bytesTotal );
      mCloudProjects[index].attachments[fileName].bytesTransferred = bytesSent;
    });

    connect( attachmentCloudReply, &QfNetworkReply::finished, this, [this, index, fileName, attachmentCloudReply]()
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
  if ( !mCloudConnection )
    return;

  int index = findProject( projectId );

  if ( index == -1 )
    return;

  mCloudProjects[index].status = ProjectStatus::Idle;

  for ( const QString &offlineLayerFileName : mCloudProjects[index].offlineLayers.keys() )
  {
    QfNetworkReply *offlineLayerReply = mCloudProjects[index].offlineLayers[offlineLayerFileName][ QStringLiteral( "cloud_reply" ) ].value<QfNetworkReply *>();

    Q_ASSERT( offlineLayerReply );

    if ( offlineLayerReply->isFinished() )
      continue;

    offlineLayerReply->abort();
  }

  for ( const QString &attachmentFileName : mCloudProjects[index].uploadAttachments.keys() )
  {
    QfNetworkReply *attachmentReply = mCloudProjects[index].uploadAttachments[attachmentFileName][ QStringLiteral( "cloud_reply" ) ].value<QfNetworkReply *>();

    Q_ASSERT( attachmentReply );

    if ( attachmentReply->isFinished() )
      continue;

    attachmentReply->abort();
  }


  if ( shouldCancelAtServer )
  {
    // CloudReply &reply = CloudReply::deleteResource( QStringLiteral( "/api/v1/deltas/%1" ) );
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
    mCloudProjects[index].modification |= ProjectModification::Local;
  else
    mCloudProjects[index].modification ^= ProjectModification::Local;

  endResetModel();
}

void QFieldCloudProjectsModel::projectListReceived()
{
  QfNetworkReply *reply = qobject_cast<QfNetworkReply *>( sender() );
  QNetworkReply *rawReply = reply->reply();

  Q_ASSERT( rawReply );

  if ( rawReply->error() != QNetworkReply::NoError )
    return;

  QByteArray response = rawReply->readAll();

  QJsonDocument doc = QJsonDocument::fromJson( response );
  QJsonArray projects = doc.array();
  reload( projects );
}

void QFieldCloudProjectsModel::downloadFile( const QString &projectId, const QString &fileName )
{
  QfNetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/files/%1/%2/" ).arg( projectId, fileName ) );
  QTemporaryFile *file = new QTemporaryFile();

  Q_ASSERT( file->open() );

//  TODO revive this
//  connect( cloudReply, &cloudReply::readyRead, this, [cloudReply, file, projectId, fileName]()
//  {
//    if ( cloudReply->error() == QNetworkReply::NoError )
//    {
//      file->write( reply->readAll() );
//    }
//  } );

  connect( reply, &QfNetworkReply::finished, this, [=]()
  {
    QNetworkReply *rawReply = reply->reply();

    bool failure = false;
    if ( rawReply->error() == QNetworkReply::NoError )
    {
//      TODO use `readyRead` as a nicer solution later
      file->write( rawReply->readAll() );

      QDir dir( QStringLiteral( "%1/%2/" ).arg( QFieldCloudUtils::localCloudDirectory(), projectId ) );

      if ( !dir.exists() )
        dir.mkpath( QStringLiteral( "." ) );

      if ( !file->copy( dir.filePath( fileName ) ) )
        failure = true;
      file->setAutoRemove( true );
    }
    else
    {
      failure = true;
    }

    int index = findProject( projectId );
    if ( index > -1 )
    {
      QVector<int> changes;

      mCloudProjects[index].downloadedSize += mCloudProjects[index].files[fileName];
      mCloudProjects[index].downloadProgress = static_cast< double >( mCloudProjects[index].downloadedSize) / mCloudProjects[index].filesSize;
      changes << DownloadProgressRole;

      if ( failure )
        mCloudProjects[index].filesFailed++;

      if ( mCloudProjects[index].downloadedSize >= mCloudProjects[index].filesSize )
      {
        mCloudProjects[index].status = ProjectStatus::Idle;
        mCloudProjects[index].localPath = QFieldCloudUtils::localProjectFilePath( projectId );
        changes << StatusRole << LocalPathRole;
        emit projectDownloaded( projectId, mCloudProjects[index].name, mCloudProjects[index].filesFailed > 0 );
      }

      QModelIndex idx = createIndex( index, 0 );
      emit dataChanged( idx, idx,  changes );
    }

    file->deleteLater();
    rawReply->deleteLater();
  } );
}

QfNetworkReply *QFieldCloudProjectsModel::uploadFile( const QString &projectId, const QString &fileName )
{
  return mCloudConnection->post( QStringLiteral( "/api/v1/files/%1/%2/" ).arg( projectId, fileName ), QVariantMap(), QStringList({fileName}) );
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
                          RemoteCheckout,
                          ProjectStatus::Idle );

    const QString projectPrefix = QStringLiteral( "QFieldCloud/projects/%1" ).arg( cloudProject.id );
    QSettings().setValue( QStringLiteral( "%1/owner" ).arg( projectPrefix ), cloudProject.owner );
    QSettings().setValue( QStringLiteral( "%1/name" ).arg( projectPrefix ), cloudProject.name );
    QSettings().setValue( QStringLiteral( "%1/description" ).arg( projectPrefix ), cloudProject.description );

    QDir localPath( QStringLiteral( "%1/%2" ).arg( QFieldCloudUtils::localCloudDirectory(), cloudProject.id ) );
    if( localPath.exists()  )
    {
      cloudProject.checkout = LocalFromRemoteCheckout;
      cloudProject.localPath = QFieldCloudUtils::localProjectFilePath( cloudProject.id );
    }

    mCloudProjects << cloudProject;
  }

  QDirIterator projectDirs( QFieldCloudUtils::localCloudDirectory(), QDir::Dirs | QDir::NoDotAndDotDot );
  while( projectDirs.hasNext() )
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

    CloudProject cloudProject( projectId, owner, name, description, LocalCheckout, ProjectStatus::Idle );
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
      return mCloudProjects.at( index.row() ).downloadProgress;
    case UploadProgressRole:
      return mCloudProjects.at( index.row() ).uploadProgress;
    case LocalPathRole:
      return mCloudProjects.at( index.row() ).localPath;
  }

  return QVariant();
}


bool QFieldCloudProjectsModel::uploadDeltas( const QString &projectId )
{
  auto projectIt = std::find_if( mCloudProjects.cbegin(), mCloudProjects.cend(), [&projectId] (const CloudProject& project) {
    return project.id == projectId;
  } );

  if ( projectIt == mCloudProjects.end() )
    return false;

  CloudProject project = mCloudProjects.at( projectIt - mCloudProjects.begin() );
  
  qDebug() << projectId << project.id;

  QDirIterator deltaFilesDirIt( QFieldCloudUtils::localCloudDirectory(), QStringList({"datafile_*.json"}), QDir::Files | QDir::NoDotAndDotDot | QDir::Readable );

}
