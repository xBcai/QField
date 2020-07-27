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

#ifndef QFIELDCLOUDCONNECTION_H
#define QFIELDCLOUDCONNECTION_H

#include "networkmanager.h"
#include "networkreply.h"

#include <QObject>
#include <QVariantMap>


class QNetworkRequest;

class QFieldCloudConnection : public QObject
{
    Q_OBJECT

  public:
    enum class ConnectionStatus
    {
      Disconnected,
      Connecting,
      LoggedIn
    };

    Q_ENUM( ConnectionStatus )

    QFieldCloudConnection();

    Q_PROPERTY( QString username READ username WRITE setUsername NOTIFY usernameChanged )
    Q_PROPERTY( QString password READ password WRITE setPassword NOTIFY passwordChanged )
    Q_PROPERTY( QString url READ url WRITE setUrl NOTIFY urlChanged )
    Q_PROPERTY( ConnectionStatus status READ status NOTIFY statusChanged )
    Q_PROPERTY( bool hasToken READ hasToken  NOTIFY tokenChanged )

    QString url() const;
    void setUrl( const QString &url );

    QString username() const;
    void setUsername( const QString &username );

    QString password() const;
    void setPassword( const QString &password );

    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();

    ConnectionStatus status() const;

    bool hasToken() { return !mToken.isEmpty(); }

    /**
     * Sends a post request with the given \a parameters to the given \a endpoint.
     *
     * If this connection is not logged in, will return nullptr.
     * The returned reply needs to be deleted by the caller.
     */
    NetworkReply *post( const QString &endpoint, const QVariantMap &params = QVariantMap(), const QStringList &fileNames = QStringList() );


    /**
     * Sends a get request to the given \a endpoint.
     *
     * If this connection is not logged in, will return nullptr.
     * The returned reply needs to be deleted by the caller.
     */
    NetworkReply *get( const QString &endpoint, const QVariantMap &params = QVariantMap() );

  signals:
    void usernameChanged();
    void passwordChanged();
    void urlChanged();
    void statusChanged();
    void tokenChanged();
    void error();

    void loginFailed( const QString &reason );

  private:
    void setStatus( ConnectionStatus status );
    void setToken( const QByteArray &token );
    void invalidateToken();
    void setAuthenticationToken( QNetworkRequest &request );

    QString mPassword;
    QString mUsername;
    QString mUrl;
    ConnectionStatus mStatus = ConnectionStatus::Disconnected;
    QByteArray mToken;

};

#endif // QFIELDCLOUDCONNECTION_H
