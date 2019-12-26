#include "qfieldcloudconnection.h"
#include <qgsnetworkaccessmanager.h>
#include <qgsapplication.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkCookieJar>
#include <QNetworkCookie>

QFieldCloudConnection::QFieldCloudConnection()
{

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
  if ( !mCsrfToken.isNull() )
  {
    request.setRawHeader( "X-CSRFToken", mCsrfToken );
  }

  QJsonObject json;

  json.insert("username", mUsername );
  json.insert("email", "email@domain.com" );
  json.insert("password", mPassword );

  QJsonDocument doc;

  doc.setObject(json);

  QByteArray requestBody = doc.toJson();

  QNetworkReply *reply = nam->post(request, requestBody);
  connect( reply, &QNetworkReply::finished, this, [this, reply, nam] () {
    QByteArray response = reply->readAll();
    const auto cookies = nam->cookieJar()->cookiesForUrl(QUrl(mUrl) );
    for ( const auto &cookie : cookies )
    {
      if ( cookie.name() == "csrftoken" )
      {
        mCsrfToken = cookie.value();
        break;
      }
    }
    setStatus( Status::LoggedIn );
    reply->deleteLater();
  } );
}

void QFieldCloudConnection::logout()
{
  QNetworkReply *reply = post( "/api/v1/auth/logout/" );
  reply->deleteLater();
  mCsrfToken.clear();
  setStatus( Status::Disconnected );
}

QFieldCloudConnection::Status QFieldCloudConnection::status() const
{
  return mStatus;
}

QNetworkReply* QFieldCloudConnection::post( const QString &endpoint, const QVariantMap& parameters )
{
  if ( mCsrfToken.isNull() )
    return nullptr;

  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkRequest request;
  request.setUrl( mUrl + endpoint );
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
  if ( !mCsrfToken.isNull() )
  {
    request.setRawHeader( "X-CSRFToken", mCsrfToken );
  }

  QJsonDocument doc( QJsonObject::fromVariantMap( parameters ) );

  QByteArray requestBody = doc.toJson();

  QNetworkReply *reply = nam->post(request, requestBody);

#if 0
  // TODO generic error handling
  connect( reply, &QNetworkReply::error, this, [ this ]( QNetworkReply::NetworkError err ) {
    emit error( err );
  } );
#endif

  return reply;
}

void QFieldCloudConnection::setStatus(Status status)
{
  if ( mStatus == status )
    return;

  mStatus = status;
  emit statusChanged();
}
