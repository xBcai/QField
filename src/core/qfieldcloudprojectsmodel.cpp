#include "qfieldcloudprojectsmodel.h"
#include "qfieldcloudconnection.h"
#include "qfieldcloudutils.h"

#include <qgis.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsapplication.h>

#include <QNetworkReply>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QDirIterator>
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

void QFieldCloudProjectsModel::refreshProjectsList()
{
  switch ( mCloudConnection->status() )
  {
    case QFieldCloudConnection::ConnectionStatus::LoggedIn:
    {
      QNetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/projects/%1/" ).arg( mCloudConnection->username() ) );
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

int QFieldCloudProjectsModel::findProject( const QString &projectId )
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
      if ( mCloudProjects.at( index ).status == ProjectStatus::Available )
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
    mCloudProjects[index].status = ProjectStatus::Downloading;
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
    }
    else
    {
      if ( index > -1 )
        mCloudProjects[index].status = ProjectStatus::Available;
      emit warning( QStringLiteral( "Error fetching project: %1" ).arg( filesReply->errorString() ) );
    }

    filesReply->deleteLater();
  } );
}

void QFieldCloudProjectsModel::connectionStatusChanged()
{
  refreshProjectsList();
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

  connect( reply, &QNetworkReply::finished, this, [=]()
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
      mCloudProjects[index].downloadProgress = static_cast< double >( mCloudProjects[index].downloadedSize) / mCloudProjects[index].filesSize;
      changes << DownloadProgressRole;

      if ( failure )
        mCloudProjects[index].filesFailed++;

      if ( mCloudProjects[index].downloadedSize >= mCloudProjects[index].filesSize )
      {
        mCloudProjects[index].status = ProjectStatus::Available;
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

QHash<int, QByteArray> QFieldCloudProjectsModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[IdRole] = "Id";
  roles[OwnerRole] = "Owner";
  roles[NameRole] = "Name";
  roles[DescriptionRole] = "Description";
  roles[StatusRole] = "Status";
  roles[DownloadProgressRole] = "DownloadProgress";
  roles[LocalPathRole] = "LocalPath";
  return roles;
}

void QFieldCloudProjectsModel::reload( const QJsonArray &remoteProjects )
{
  beginResetModel();
  mCloudProjects .clear();

  for ( const auto project : remoteProjects )
  {
    QVariantHash projectDetails = project.toObject().toVariantHash();
    CloudProject cloudProject( projectDetails.value( "id" ).toString(),
                          projectDetails.value( "owner" ).toString(),
                          projectDetails.value( "name" ).toString(),
                          projectDetails.value( "description" ).toString(),
                          ProjectStatus::Available );

    QDir localPath( QStringLiteral( "%1/%2" ).arg( QFieldCloudUtils::localCloudDirectory(), cloudProject.id ) );
    if( localPath.exists()  )
      cloudProject.localPath = QFieldCloudUtils::localProjectFilePath( cloudProject.id );

    mCloudProjects << cloudProject;
  }

  QDirIterator ownerDirs( QFieldCloudUtils::localCloudDirectory(), QDir::Dirs | QDir::NoDotAndDotDot );
  while( ownerDirs.hasNext() )
  {
    ownerDirs.next();
    QDirIterator projectNameDirs( ownerDirs.filePath(), QDir::Dirs | QDir::NoDotAndDotDot );
    while( projectNameDirs.hasNext() )
    {
      projectNameDirs.next();
      int index = findProject( ownerDirs.fileName() );
      if ( index == -1 )
      {
        CloudProject cloudProject( QString(),
                                   ownerDirs.fileName(),
                                   projectNameDirs.fileName(),
                                   QString(),
                                   ProjectStatus::LocalOnly );
        cloudProject.localPath = QFieldCloudUtils::localProjectFilePath( cloudProject.id );
        mCloudProjects << cloudProject;
      }
    }
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
    case StatusRole:
      return static_cast<int>( mCloudProjects.at( index.row() ).status );
    case DownloadProgressRole:
      return mCloudProjects.at( index.row() ).downloadProgress;
    case LocalPathRole:
      return mCloudProjects.at( index.row() ).localPath;
  }

  return QVariant();
}
