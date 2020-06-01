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

#include <QObject>
#include <QVariantMap>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QRandomGenerator>

class QNetworkReply;
class QgsNetworkAccessManager;


class CloudReply : public QObject
{

  Q_OBJECT

public:

  /**
   * Makes a new GET request
   * @param nam
   * @param request
   * @return
   */
  static CloudReply *get( QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request );


  /**
   * Makes a new POST request
   * @param nam
   * @param request
   * @return
   */
  static CloudReply *post( QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, const QByteArray payload = QByteArray() );


  /**
   * Makes a new POST request
   * @param nam
   * @param request
   * @return
   */
  static CloudReply *post( QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, QHttpMultiPart *payload );


  /**
   * A wrapper around QNetworkReply that allows retryable requests.
   * @param op HTTP method
   * @param nam network access manager
   * @param request the request to be performed
   * @param payload the request payload
   */
  CloudReply( const QNetworkAccessManager::Operation operation, QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, const QByteArray payloadByteArray );


  /**
   * A wrapper around QNetworkReply that allows retryable requests.
   * @param op HTTP method
   * @param nam network access manager
   * @param request the request to be performed
   * @param payload the request payload
   */
  CloudReply( const QNetworkAccessManager::Operation operation, QgsNetworkAccessManager *networkAccessManager, const QNetworkRequest request, QHttpMultiPart *payloadMultiPart );


  /**
   * Aborts the current request and any other retries. Makes the current object into a final state.
   */
  void abort();


  /**
   * Get the QNetworkReply object once the CloudReply is finilized.
   * @return network reply
   */
  QNetworkReply *reply() const;


  /**
   * Reimplements QNetworkReply::ignoreSslErrors.
   * @param error a list of error to be ignored.
   */
  void ignoreSslErrors( QList<QSslError> errors );


  /**
   * Whether the request reached a final status.
   * @return true if the request reached a final status.
   */
  bool isFinished() const;


signals:

  /**
   * Replicates `QNetworkReply::downloadProgress` signal.
   * @note Because download may fail mid request and then retried, the bytesReceived may be reset back to 0.
   * @param bytesSent
   * @param bytesTotal
   */
  void downloadProgress( int bytesReceived, int bytesTotal );


  /**
   * Replicates `QNetworkReply::uploadProgress` signal.
   * @note Because upload may fail mid request and then retried, the bytesSent may be reset back to 0.
   * @param bytesSent
   * @param bytesTotal
   */
  void uploadProgress( int bytesSent, int bytesTotal );


  /**
   * Replicates `QNetworkReply::encrypted` signal.
   * @note May be called multiple times for each retry.
   */
  void encrypted();


  /**
   * Replicates `QNetworkReply::finished` signal. It is called only once, when the request was successfull, got a final error or ran out of retries.
   */
  void finished();


  // /////////////////
  // more than QNetworkReply signals
  // /////////////////
  /**
   * Emitted when a new retry is initiated.
   */
  void retry();


  /**
   * Emitted when a new error has occured.
   * @param code
   */
  void errorOccurred( QNetworkReply::NetworkError code );


  /**
   * Emitted when a new temporary error has occured. This is basically emitting the error that has occured during a retry.
   * @param code
   */
  void temporaryErrorOccurred( QNetworkReply::NetworkError code );

private:

  /**
   * The current HTTP method.
   */
  QNetworkAccessManager::Operation mOperation;


  /**
   * Whether the cloud reply has reached a final state.
   */
  bool mIsFinished = false;


  /**
   * Whether it is a multi-part request
   */
  bool mIsMultiPartPayload = false;


  /**
   * Number of retries left. Once the value reaches zero, the status of the last reply is the final status.
   */
  int mRetriesLeft = 5;


  /**
   * Upper bound of the delay between retries in milliseconds.
   */
  int mMaxTimeoutBetweenRetriesMs = 2000;


  /**
   * Expected SSL errors to be ignored.
   */
  QList<QSslError> mExpectedSslErrors;


  /**
   * Random number generator instance. Used to create random delay bettween retries.
   */
  QRandomGenerator mRNG;


  /**
   * Network access manager.
   */
  QgsNetworkAccessManager *mNetworkAccessManager = nullptr;


  /**
   * The current request.
   */
  QNetworkRequest mRequest;


  /**
   * Request payload
   */
  const QByteArray mPayloadByteArray;


  /**
   * Request payload as multipart
   */
  QHttpMultiPart *mPayloadMultiPart;


  /**
   * The current outgoing request. If the request fails and can be retried, the object is disposed and replaced with a new one.
   */
  QNetworkReply *mReply = nullptr;


  /**
   * Binds signal listeners to `QNetworkReply` object
   */
  void initiateRequest();


  /**
   * Reemits `QNetworkReply::downloadProgress` signal.
   * @note Because download may fail mid request and then retried, the bytesSent may be reset back to 0.
   * @param bytesReceived
   * @param bytesTotal
   */
  void onDownloadProgress( int bytesReceived, int bytesTotal );


  /**
   * Reemits `QNetworkReply::uploadProgress` signal.
   * @note Because upload may fail mid request and then retried, the bytesSent may be reset back to 0.
   * @param bytesSent
   * @param bytesTotal
   */
  void onUploadProgress( int bytesSent, int bytesTotal );


  /**
   * Reemits `QnetworkReply::encrypted` signal.
   */
  void onEncrypted();


  /**
   * Called when a request attempt is finished. If needed, make a retry.
   */
  void onFinished();
};


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
    CloudReply *post( const QString &endpoint, const QVariantMap &params = QVariantMap(), const QStringList &fileNames = QStringList() );


    /**
     * Sends a get request to the given \a endpoint.
     *
     * If this connection is not logged in, will return nullptr.
     * The returned reply needs to be deleted by the caller.
     */
    CloudReply *get( const QString &endpoint, const QVariantMap &params = QVariantMap() );

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
