#include "qfieldcloudprojectsmodel.h"
#include "qfieldcloudconnection.h"

#include <qgis.h>
#include <qgsnetworkaccessmanager.h>
#include <qgsapplication.h>

#include <QNetworkReply>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QDebug>

QFieldCloudProjectsModel::QFieldCloudProjectsModel()
{

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
  QNetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/projects/%1/" ).arg( mCloudConnection->username() ) );
  connect( reply, &QNetworkReply::finished, this, &QFieldCloudProjectsModel::projectListReceived );
}

void QFieldCloudProjectsModel::download( const QString &owner, const QString &projectName )
{
  if ( !mCloudConnection )
    return;

  QNetworkReply *filesReply = mCloudConnection->get( QStringLiteral( "/api/v1/projects/%1/%2/files/" ).arg( owner, projectName ) );

  connect( filesReply, &QNetworkReply::finished, this, [filesReply, this, owner, projectName]()
  {
    if ( filesReply->error() == QNetworkReply::NoError )
    {
      QJsonArray files = QJsonDocument::fromJson( filesReply->readAll() ).array();

      for ( const auto file : files )
      {
        downloadFile( owner, projectName, file.toObject().value( QStringLiteral( "name" ) ).toString() );
      }
    }
    else
    {
      emit warning( QStringLiteral( "Error fetching project: %1" ).arg( filesReply->errorString() ) );
    }

    filesReply->deleteLater();
  } );
}

void QFieldCloudProjectsModel::connectionStatusChanged()
{
  if ( mCloudConnection->status() == QFieldCloudConnection::Status::LoggedIn )
  {
    refreshProjectsList();
  }
}

void QFieldCloudProjectsModel::projectListReceived()
{
  QNetworkReply *reply = qobject_cast<QNetworkReply *>( sender() );
  Q_ASSERT( reply );

  if ( reply->error() != QNetworkReply::NoError )
    return;

  clear();

  QByteArray response = reply->readAll();

  QJsonDocument doc = QJsonDocument::fromJson( response );
  QJsonArray projects = doc.array();

  for ( const auto project : projects )
  {
    QVariantHash projectDetails = project.toObject().toVariantHash();

    QStandardItem *item = new QStandardItem();
    item->setData( projectDetails.value( "id" ), IdRole );
    item->setData( mCloudConnection ? mCloudConnection->username() : QString(), OwnerRole ); //TODO: server API should return that
    item->setData( projectDetails.value( "name" ), NameRole );
    item->setData( projectDetails.value( "description" ), DescriptionRole );
    item->setData( QVariant::fromValue<Status>( Status::Available ), StatusRole );
    insertRow( rowCount(), item );
  }
}

void QFieldCloudProjectsModel::downloadFile( const QString &owner, const QString &projectName, const QString &fileName )
{
  QNetworkReply *reply = mCloudConnection->get( QStringLiteral( "/api/v1/projects/%1/%2/%3/" ).arg( owner, projectName, fileName ) );

  QTemporaryFile *file = new QTemporaryFile();
  file->open();

  connect( reply, &QNetworkReply::readyRead, this, [reply, file, owner, projectName, fileName]()
  {
    if ( reply->error() == QNetworkReply::NoError )
    {
      file->write( reply->readAll() );
    }
  } );

  connect( reply, &QNetworkReply::finished, this, [this, reply, file, owner, projectName, fileName]()
  {
    file->close();
    if ( reply->error() == QNetworkReply::NoError )
    {
      QString settingsDirPath = QgsApplication::qgisSettingsDirPath();
      if ( settingsDirPath.right( 1 ) == "/" )
        settingsDirPath = settingsDirPath.left( settingsDirPath.length() - 1);
      QDir dir( QStringLiteral( "%1/cloud_projects/%2/%3/" ).arg( settingsDirPath, owner, projectName ) );

      if ( !dir.exists() )
        dir.mkpath( QStringLiteral( "." ) );

      file->copy( dir.filePath( fileName ) );
      file->setAutoRemove( true );

      emit warning( QStringLiteral( "File written to %1" ).arg( dir.filePath( fileName ) ) );
    }
    else
    {
      emit warning( QStringLiteral( "Error fetching project file: %1" ).arg( reply->errorString() ) );
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
  return roles;
}
