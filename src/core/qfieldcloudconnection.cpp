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
#include <QTimer>
#include <QUrlQuery>
#include <QFile>


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

  // TODO remove this!!! temporary SSL workaround
  connect( reply, &QNetworkReply::sslErrors, this, [ = ]( const QList<QSslError> &errors )
  {
    for ( const QSslError &error : errors )
      qDebug() << "SSL: " << error;

    reply->ignoreSslErrors( errors );
  } );

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

NetworkReply *QFieldCloudConnection::post( const QString &endpoint, const QVariantMap &params, const QStringList &fileNames )
{
  if ( mToken.isNull() )
    return nullptr;

  QNetworkRequest request( mUrl + endpoint );
  QByteArray requestBody = QJsonDocument( QJsonObject::fromVariantMap( params ) ).toJson();
  setAuthenticationToken( request );

  if ( fileNames.isEmpty() )
  {
    request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );

    return NetworkManager::post( request, requestBody );
  }

  QHttpMultiPart *multiPart = new QHttpMultiPart( QHttpMultiPart::FormDataType );
  QHttpPart textPart;

  QJsonDocument doc( QJsonObject::fromVariantMap( params ) );
  textPart.setHeader( QNetworkRequest::ContentTypeHeader, QVariant( "application/json" ) );
  textPart.setHeader( QNetworkRequest::ContentDispositionHeader, QVariant( "form-data; name=\"text\"" ) );
  textPart.setBody( doc.toJson() );

  multiPart->append( textPart );

  for ( const QString &fileName : fileNames )
  {
    QHttpPart filePart;
    QFile *file = new QFile( fileName, multiPart );

    if ( ! file->open( QIODevice::ReadOnly ) )
      return nullptr;

    const QString header = QStringLiteral( "form-data; name=\"file\"; filename=\"%1\"" ).arg( fileName );
    filePart.setHeader( QNetworkRequest::ContentTypeHeader, QVariant( "application/json" ) );
    filePart.setHeader( QNetworkRequest::ContentDispositionHeader, header );
    filePart.setBodyDevice( file );

    multiPart->append( filePart );
  }

  NetworkReply *reply = NetworkManager::post( request, multiPart );

  multiPart->setParent( reply );

  return reply;
}

NetworkReply *QFieldCloudConnection::get( const QString &endpoint, const QVariantMap &params )
{
  QNetworkRequest request;
  QUrl url( mUrl + endpoint );
  QUrlQuery urlQuery;

  QMap<QString, QVariant>::const_iterator it = params.constBegin();
  QMap<QString, QVariant>::const_iterator itEnd = params.constEnd();

  while ( it != itEnd ) {
    urlQuery.addQueryItem( it.key(), it.value().toString() );
    ++it;
  }

  url.setQuery( urlQuery );

  request.setUrl( url );
  request.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
  setAuthenticationToken( request );

  return NetworkManager::get( request );
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

