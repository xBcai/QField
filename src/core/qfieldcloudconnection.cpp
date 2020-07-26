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
#include <QSettings>

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
  QNetworkReply *reply = post( "/api/v1/auth/logout/" );
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

