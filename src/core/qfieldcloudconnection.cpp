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
  setToken( QByteArray() ); // invalidate token on username change

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
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );

  QNetworkReply *reply;
  if ( mPassword.isEmpty() )
  {
    request.setUrl( mUrl + "/api/v1/users/user/" );
    setAuthenticationToken( request );
    reply = nam->get( request );
  }
  else
  {
    QJsonObject json;
    request.setUrl( mUrl + "/api/v1/auth/token/" );
    json.insert( "username", mUsername );
    json.insert( "password", mPassword );
    QJsonDocument doc;
    doc.setObject( json );
    QByteArray requestBody = doc.toJson();
    reply = nam->post( request, requestBody );
  }

  setStatus( Status::Connecting );

  connect( reply, &QNetworkReply::finished, this, [this, reply]()
  {
    if ( reply->error() == QNetworkReply::NoError )
    {
      QByteArray response = reply->readAll();

      QByteArray token = QJsonDocument::fromJson( response ).object().toVariantMap().value( QStringLiteral( "token" ) ).toByteArray();
      if ( !token.isEmpty() )
      {
        setToken( token );
        QSettings().setValue( "/QFieldCloud/token", token );
      }

      mUsername  = QJsonDocument::fromJson( response ).object().toVariantMap().value( QStringLiteral( "username" ) ).toString();
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
  connect( reply, &QNetworkReply::finished, this, [reply]()
  {
    reply->deleteLater();
  } );

  mPassword.clear();
  setToken( QByteArray() );
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

void QFieldCloudConnection::setToken( const QByteArray &token )
{
  if ( mToken == token )
    return;

  mToken = token;
  emit tokenChanged();
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

