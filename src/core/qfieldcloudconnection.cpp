#include "qfieldcloudconnection.h"
#include <qgsnetworkaccessmanager.h>
#include <qgsapplication.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QSettings>

QFieldCloudConnection::QFieldCloudConnection()
{
  mUsername = QSettings().value( "/QFieldCloud/username" ).toByteArray();
  mToken = QSettings().value( "/QFieldCloud/token" ).toByteArray();
}

QString QFieldCloudConnection::url() const
{
  return mUrl;
}

void QFieldCloudConnection::setUrl( const QString &url )
{
  if ( url == mUrl )
    return;

  mUrl = url;
  emit urlChanged();
}

QString QFieldCloudConnection::username() const
{
  return mUsername;
}

void QFieldCloudConnection::setUsername( const QString &username )
{
  if ( mUsername == username )
    return;

  mUsername = username;
  mToken.clear(); // invalidate token on username change

  emit usernameChanged();
}

QString QFieldCloudConnection::password() const
{
  return mPassword;
}

void QFieldCloudConnection::setPassword( const QString &password )
{
  if ( password == mPassword )
    return;

  mPassword = password;
  emit passwordChanged();

}

void QFieldCloudConnection::login()
{
  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkRequest request;
  request.setUrl( mUrl + "/api/v1/auth/login/" );
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );

  QJsonObject json;

  if ( mPassword.isEmpty() )
    setAuthenticationToken( request );
  else
  {
    json.insert( "username", mUsername );
    json.insert( "password", mPassword );
  }

  setStatus( Status::Connecting );

  QJsonDocument doc;
  doc.setObject( json );
  QByteArray requestBody = doc.toJson();
  QNetworkReply *reply = nam->post( request, requestBody );
  connect( reply, &QNetworkReply::finished, this, [this, reply]()
  {
    if ( reply->error() == QNetworkReply::NoError )
    {
      QByteArray response = reply->readAll();

      const QVariant &key = QJsonDocument::fromJson( response ).object().toVariantMap().value( QStringLiteral( "key" ) );

      mToken = key.toByteArray();
      QSettings().setValue( "/QFieldCloud/username", mUsername );
      QSettings().setValue( "/QFieldCloud/token", key );

      setStatus( Status::LoggedIn );
    }
    else
    {
      emit loginFailed( QStringLiteral( "%1 (HTTP Status %2)" ).arg( reply->errorString(), QString::number( reply->error() ) ) );
      setStatus( Status::Disconnected );
    }
    reply->deleteLater();
  } );
}

void QFieldCloudConnection::logout()
{
  QNetworkReply *reply = post( "/api/v1/auth/logout/" );
  reply->deleteLater();
  mPassword.clear();
  mToken.clear();
  QSettings().remove( "/QFieldCloud/username" );
  QSettings().remove( "/QFieldCloud/token" );
  setStatus( Status::Disconnected );
}

QFieldCloudConnection::Status QFieldCloudConnection::status() const
{
  return mStatus;
}

QNetworkReply *QFieldCloudConnection::post( const QString &endpoint, const QVariantMap &parameters )
{
  if ( mToken.isNull() )
    return nullptr;

  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkRequest request;
  request.setUrl( mUrl + endpoint );
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
  setAuthenticationToken( request );

  QJsonDocument doc( QJsonObject::fromVariantMap( parameters ) );

  QByteArray requestBody = doc.toJson();

  QNetworkReply *reply = nam->post( request, requestBody );

#if 0
  // TODO generic error handling
  connect( reply, &QNetworkReply::error, this, [ this ]( QNetworkReply::NetworkError err )
  {
    emit error( err );
  } );
#endif

  return reply;
}

QNetworkReply *QFieldCloudConnection::get( const QString &endpoint )
{
  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkRequest request;
  request.setUrl( mUrl + endpoint );
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
  setAuthenticationToken( request );

  QNetworkReply *reply = nam->get( request );

  return reply;
}

void QFieldCloudConnection::setStatus( Status status )
{
  if ( mStatus == status )
    return;

  mStatus = status;
  emit statusChanged();
}

void QFieldCloudConnection::setAuthenticationToken( QNetworkRequest &request )
{
  if ( !mToken.isNull() )
  {
    request.setRawHeader( "Authorization", "Token " + mToken );
  }
}

void QFieldCloudConnection::checkStatus()
{
  if ( mToken.isNull() )
  {
    setStatus( Status::Disconnected );
    return;
  }

  QNetworkReply *reply = get( "/api/v1/projects/mkuhn/" );

  connect( reply, &QNetworkReply::finished, this, [this, reply]()
  {
    if ( reply->error() == QNetworkReply::NoError )
      setStatus( Status::LoggedIn );
    else
      emit loginFailed( QStringLiteral( "%1 (HTTP Status %2)" ).arg( reply->errorString(), QString::number( reply->error() ) ) );
  } );
}
