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
      QNetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/projects/" ) );
      connect( reply, &QNetworkReply::finished, this, &QFieldCloudProjectsModel::projectListReceived );
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

  QNetworkReply *filesReply = mCloudConnection->get( QStringLiteral( "/api/v1/files/%1/" ).arg( projectId ) );

  connect( filesReply, &QNetworkReply::finished, this, [filesReply, this, projectId]()
  {
    int index = findProject( projectId );
    if ( filesReply->error() == QNetworkReply::NoError )
    {
      const QJsonArray files = QJsonDocument::fromJson( filesReply->readAll() ).array();
      for ( const auto file : files )
      {
        QJsonObject fileObject = file.toObject();
        QString fileName = fileObject.value( QStringLiteral( "name" ) ).toString();
        if ( index > -1 )
        {
          int fileSize = fileObject.value( QStringLiteral( "size" ) ).toInt();
          mCloudProjects[index].files.insert( fileName, fileSize );
          mCloudProjects[index].filesSize += fileSize;
        }
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
      emit warning( QStringLiteral( "Error fetching project: %1" ).arg( filesReply->errorString() ) );
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

  if ( ! ( mCloudProjects[index].modification & ProjectModification::Local ) )
    return;

  mCloudProjects[index].filesFailed = 0;
  mCloudProjects[index].uploadedSize = 0;
  mCloudProjects[index].uploadProgress = 0.0;
  mCloudProjects[index].status = ProjectStatus::Uploading;

  QModelIndex idx = createIndex( index, 0 );
  emit dataChanged( idx, idx,  QVector<int>() << StatusRole << UploadProgressRole );

  DeltaFileWrapper deltaFile( QgsProject::instance(), QStringLiteral( "%1/deltafile_commited.json" ).arg( QgsProject::instance()->homePath() ) );

  if ( deltaFile.hasError() )
  {
    QgsLogger::warning( QStringLiteral( "The delta file has an error: %1" ).arg( deltaFile.errorString() ) );
    return;
  }

  QNetworkReply *deltasReply = mCloudConnection->post( QStringLiteral( "/api/v1/deltas/%1/" ).arg( projectId ), QVariantMap({
    {"data", deltaFile.toJson()}
  }) );

  if ( ! deltasReply )
    return;

  connect( deltasReply, &QNetworkReply::finished, this, [deltasReply, this, projectId]()
  {
    DeltaFileWrapper deltaFile( QgsProject::instance(), QStringLiteral( "%1/deltafile_commited.json" ).arg( QgsProject::instance()->homePath() ) );
    int index = findProject( projectId );
    
    if ( deltasReply->error() == QNetworkReply::NoError )
    {
      const QStringList fileNames = deltaFile.attachmentFileNames().keys();

      for ( const QString &fileName : fileNames )
      {
        const QFileInfo fi( fileName );

        mCloudProjects[index].files.insert( fileName, fi.size() );
        mCloudProjects[index].filesSize += fi.size();

        uploadFile( projectId, fileName );
      }
    }
    else
    {
      if ( index > -1 )
        mCloudProjects[index].modification |= ProjectModification::Local;

      emit warning( QStringLiteral( "Error uploading project: %1" ).arg( deltasReply->errorString() ) );
    }
  } );
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

  if ( layerObserver()->isDirty() )
    mCloudProjects[index].modification |= ProjectModification::Local;
  else
    mCloudProjects[index].modification ^= ProjectModification::Local;

  endResetModel();
}

void QFieldCloudProjectsModel::projectListReceived()
{
  QNetworkReply *reply = qobject_cast<QNetworkReply *>( sender() );
  Q_ASSERT( reply );

  if ( reply->error() != QNetworkReply::NoError )
    return;

  QByteArray response = reply->readAll();

  QJsonDocument doc = QJsonDocument::fromJson( response );
  QJsonArray projects = doc.array();
  reload( projects );
}

void QFieldCloudProjectsModel::downloadFile( const QString &projectId, const QString &fileName )
{
  QNetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/files/%1/%2/" ).arg( projectId, fileName ) );

  QTemporaryFile *file = new QTemporaryFile();
  file->open();

  connect( reply, &QNetworkReply::readyRead, this, [reply, file, projectId, fileName]()
  {
    if ( reply->error() == QNetworkReply::NoError )
    {
      file->write( reply->readAll() );
    }
  } );

  connect( reply, &QNetworkReply::finished, this, [ = ]()
  {
    bool failure = false;
    if ( reply->error() == QNetworkReply::NoError )
    {
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
      mCloudProjects[index].downloadProgress = static_cast< double >( mCloudProjects[index].downloadedSize ) / mCloudProjects[index].filesSize;
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
    reply->deleteLater();
  } );
}

void QFieldCloudProjectsModel::uploadFile( const QString &projectId, const QString &fileName )
{
  QNetworkReply *reply = mCloudConnection->post( QStringLiteral( "/api/v1/files/%1/%2/" ).arg( projectId, fileName ), QVariantMap(), QStringList({fileName}) );

  connect( reply, &QNetworkReply::readyRead, this, [reply, projectId, fileName]()
  {
    if ( reply->error() == QNetworkReply::NoError )
    {
      // TODO finish this
    }
  } );
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

  QStringList deltaFileNames;

  while ( deltaFilesDirIt.hasNext() ) 
  {
    deltaFileNames << deltaFilesDirIt.filePath();
    deltaFilesDirIt.next();
  }

  if ( deltaFileNames.size() == 0 )
    return false;


  // TODO start the actual upload of the files


  return true;
}
