/***************************************************************************
    qfieldcloudconnection.cpp
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

#include "qfieldcloudconnection.h"
#include <qgsnetworkaccessmanager.h>
#include <qgsapplication.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkCookieJar>
#include <QNetworkCookie>
#include <QHttpMultiPart>
#include <QSettings>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrlQuery>
#include <QFile>


CloudReply::CloudReply( const QNetworkAccessManager::Operation operation, QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, const QByteArray payloadByteArray = QByteArray() ):
  mOperation( operation ),
  mNetworkAccessManager( networkAccessManager ),
  mRequest( request ),
  mPayloadByteArray( payloadByteArray )
{
  mIsMultiPartPayload = false;

  initiateRequest();
};


CloudReply::CloudReply( const QNetworkAccessManager::Operation operation, QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, QHttpMultiPart *payloadMultiPart ):
  mOperation( operation ),
  mNetworkAccessManager( networkAccessManager ),
  mRequest( request ),
  mPayloadMultiPart( payloadMultiPart )
{
  mIsMultiPartPayload = true;

  initiateRequest();
};


CloudReply *CloudReply::get( QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request )
{
  return new CloudReply( QNetworkAccessManager::GetOperation, networkAccessManager, request);
}


CloudReply *CloudReply::post( QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, const QByteArray payload )
{
  return new CloudReply( QNetworkAccessManager::PostOperation, networkAccessManager, request, payload );
}


CloudReply *CloudReply::post( QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, QHttpMultiPart *payload )
{
  return new CloudReply( QNetworkAccessManager::PostOperation, networkAccessManager, request, payload );
}


void CloudReply::abort()
{
  mIsFinished = true;
  mReply->abort();
}


QNetworkReply *CloudReply::reply() const
{
  if ( mIsFinished )
    return mReply;

  return nullptr;
}


void CloudReply::ignoreSslErrors( QList<QSslError> errors )
{
  mExpectedSslErrors = errors;
}


bool CloudReply::isFinished() const
{
  return mIsFinished;
}


void CloudReply::initiateRequest()
{
  switch ( mOperation ) {
    case QNetworkAccessManager::HeadOperation:
      mReply = mNetworkAccessManager->head( mRequest );
      break;
    case QNetworkAccessManager::GetOperation:
      mReply = mNetworkAccessManager->get( mRequest );
      break;
    case QNetworkAccessManager::PutOperation:
      if ( mIsMultiPartPayload )
        mReply = mNetworkAccessManager->put( mRequest, mPayloadMultiPart );
      else
        mReply = mNetworkAccessManager->put( mRequest, mPayloadByteArray );
      break;
    case QNetworkAccessManager::PostOperation:
      if ( mIsMultiPartPayload )
        mReply = mNetworkAccessManager->post( mRequest, mPayloadMultiPart );
      else
        mReply = mNetworkAccessManager->post( mRequest, mPayloadByteArray );
      break;
    case QNetworkAccessManager::DeleteOperation:
      mReply = mNetworkAccessManager->deleteResource( mRequest );
      break;
    case QNetworkAccessManager::CustomOperation:
      throw QStringLiteral( "Not implemented!" );
    case QNetworkAccessManager::UnknownOperation:
      throw QStringLiteral( "Not implemented!" );
  }

  mReply->ignoreSslErrors( mExpectedSslErrors );

  connect( mReply, &QNetworkReply::finished, this, &CloudReply::onFinished );
  connect( mReply, &QNetworkReply::encrypted, this, &CloudReply::onEncrypted );
  connect( mReply, &QNetworkReply::downloadProgress, this, &CloudReply::onDownloadProgress );
  connect( mReply, &QNetworkReply::uploadProgress, this, &CloudReply::onUploadProgress );
}


void CloudReply::onDownloadProgress(int bytesReceived, int bytesTotal)
{
  emit uploadProgress( bytesReceived, bytesTotal );
}


void CloudReply::onUploadProgress(int bytesSent, int bytesTotal)
{
  emit uploadProgress( bytesSent, bytesTotal );
}


void CloudReply::onEncrypted()
{
  emit encrypted();
}


void CloudReply::onFinished()
{
  bool canRetry = false;
  QNetworkReply::NetworkError error = mReply->error();

  switch ( error )
  {
    case QNetworkReply::NoError:
      mIsFinished = true;
      emit finished();
      return;
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::InternalServerError:
    case QNetworkReply::ContentReSendError:
    case QNetworkReply::ServiceUnavailableError:
      canRetry = true;
      break;
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::OperationCanceledError:
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::BackgroundRequestNotAllowedError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentOperationNotPermittedError:
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::ContentGoneError:
    case QNetworkReply::ContentConflictError:
    case QNetworkReply::OperationNotImplementedError:
    case QNetworkReply::ProtocolUnknownError:
    case QNetworkReply::ProtocolInvalidOperationError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::UnknownProxyError:
    case QNetworkReply::UnknownContentError:
    case QNetworkReply::ProtocolFailure:
    case QNetworkReply::UnknownServerError:
      canRetry = false;
      break;
    default:
      canRetry = false;
      break;
  }

  if ( ! canRetry || mRetriesLeft == 0 )
  {
    mIsFinished = true;

    emit errorOccurred( error );
    emit finished();

    return;
  }

  emit temporaryErrorOccurred( error );

  // wait random time before the retry is sent
  QTimer::singleShot( mRNG.bounded( mMaxTimeoutBetweenRetriesMs ), this, [ = ] () {
    emit retry();

    mRetriesLeft--;

    initiateRequest();
  } );
}


QFieldCloudConnection::QFieldCloudConnection()
  : mToken( QSettings().value( "/QFieldCloud/token" ).toByteArray() )
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
  invalidateToken();

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
    request.setUrl( mUrl + "/api/v1/auth/user/" );
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

  setStatus( ConnectionStatus::Connecting );

  connect( reply, &QNetworkReply::finished, this, [this, reply]()
  {
    if ( reply->error() == QNetworkReply::NoError )
    {
      QByteArray response = reply->readAll();
      QByteArray token = QJsonDocument::fromJson( response ).object().toVariantMap().value( QStringLiteral( "token" ) ).toByteArray();
      if ( !token.isEmpty() )
      {
        setToken( token );
      }

      mUsername  = QJsonDocument::fromJson( response ).object().toVariantMap().value( QStringLiteral( "username" ) ).toString();
      setStatus( ConnectionStatus::LoggedIn );
    }
    else
    {
      emit loginFailed( QStringLiteral( "%1 (HTTP Status %2)" ).arg( reply->errorString(), QString::number( reply->error() ) ) );
      setStatus( ConnectionStatus::Disconnected );
    }
    reply->deleteLater();
  } );
}

void QFieldCloudConnection::logout()
{
  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkRequest request( mUrl + QStringLiteral( "/api/v1/auth/logout/" ) );
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
  setAuthenticationToken( request );

  QNetworkReply *reply = nam->post( request, QByteArray() );

  connect( reply, &QNetworkReply::finished, this, [reply]()
  {
    reply->deleteLater();
  } );

  mPassword.clear();
  invalidateToken();
  QSettings().remove( "/QFieldCloud/token" );

  setStatus( ConnectionStatus::Disconnected );
}

QFieldCloudConnection::ConnectionStatus QFieldCloudConnection::status() const
{
  return mStatus;
}

CloudReply *QFieldCloudConnection::post( const QString &endpoint, const QVariantMap &params, const QStringList &fileNames )
{
  if ( mToken.isNull() )
    return nullptr;

  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkRequest request( mUrl + endpoint );
  setAuthenticationToken( request );

  if ( fileNames.isEmpty() )
  {
    request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );

    QJsonDocument doc( QJsonObject::fromVariantMap( params ) );

    QByteArray requestBody = doc.toJson();

    return CloudReply::post( nam, request, requestBody );
  }

  QHttpMultiPart *multiPart = new QHttpMultiPart( QHttpMultiPart::FormDataType );
  QByteArray requestBody = QJsonDocument( QJsonObject::fromVariantMap( params ) ).toJson();
  QHttpPart textPart;

  textPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("application/json"));
  textPart.setBody("toto");/* toto is the name I give to my file in the server */

  for ( const QString &fileName : fileNames )
  {
    QHttpPart imagePart;
    QFile *file = new QFile( fileName );
    file->setParent( multiPart );

    if ( ! file->open(QIODevice::ReadOnly) )
      return nullptr;

    const QString header = QStringLiteral( "form-data; name=\"file\"; filename=\"%1\"" ).arg( fileName );
    imagePart.setHeader( QNetworkRequest::ContentDispositionHeader, QVariant( fileName ) );
    imagePart.setBodyDevice( file );
    multiPart->append( imagePart );
  }

  CloudReply *reply = CloudReply::post( nam, request, multiPart );

  multiPart->setParent( reply );

  return reply;
}

CloudReply *QFieldCloudConnection::get( const QString &endpoint, const QVariantMap &params )
{
  QgsNetworkAccessManager *nam = QgsNetworkAccessManager::instance();
  QNetworkRequest request;
  QUrl url( mUrl + endpoint );
  QUrlQuery urlQuery;

  QMap<QString, QVariant>::const_iterator i = params.begin();
  while (i != params.end())
    urlQuery.addQueryItem( i.key(), i.value().toString() );

  request.setUrl( url );
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
  setAuthenticationToken( request );

  return CloudReply::get( nam, request );
}

void QFieldCloudConnection::setToken( const QByteArray &token )
{
  if ( mToken == token )
    return;

  mToken = token;
  QSettings().setValue( "/QFieldCloud/token", token );

  emit tokenChanged();
}

void QFieldCloudConnection::invalidateToken()
{
  if ( mToken.isNull() )
    return;

  mToken = QByteArray();
  emit tokenChanged();
}

void QFieldCloudConnection::setStatus( ConnectionStatus status )
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

