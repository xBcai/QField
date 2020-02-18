#ifndef QFIELDCLOUDCONNECTION_H
#define QFIELDCLOUDCONNECTION_H

#include <QObject>
#include <QVariantMap>
#include <QNetworkRequest>

class QNetworkReply;

class QFieldCloudConnection : public QObject
{
    Q_OBJECT

  public:
    enum class Status
    {
      Disconnected,
      Connecting,
      LoggedIn
    };

    Q_ENUM( Status )

    QFieldCloudConnection();

    Q_PROPERTY( QString username READ username WRITE setUsername NOTIFY usernameChanged )
    Q_PROPERTY( QString password READ password WRITE setPassword NOTIFY passwordChanged )
    Q_PROPERTY( QString url READ url WRITE setUrl NOTIFY urlChanged )
    Q_PROPERTY( Status status READ status NOTIFY statusChanged )
    Q_PROPERTY( bool hasToken READ hasToken  NOTIFY tokenChanged )

    QString url() const;
    void setUrl( const QString &url );

    QString username() const;
    void setUsername( const QString &username );

    QString password() const;
    void setPassword( const QString &password );

    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();

    Status status() const;

    bool hasToken() { return !mToken.isEmpty(); }

    /**
     * Sends a post request with the given \a parameters to the given \a endpoint.
     *
     * If this connection is not logged in, will return nullptr.
     * The returned reply needs to be deleted by the caller.
     */
    QNetworkReply *post( const QString &endpoint, const QVariantMap &parameters = QVariantMap() );

    /**
     * Sends a get request to the given \a endpoint.
     *
     * If this connection is not logged in, will return nullptr.
     * The returned reply needs to be deleted by the caller.
     */
    QNetworkReply *get( const QString &endpoint );

  signals:
    void usernameChanged();
    void passwordChanged();
    void urlChanged();
    void statusChanged();
    void tokenChanged();
    void error();

    void loginFailed( const QString &reason );

  private:
    void setStatus( Status status );
    void setToken( const QByteArray &token );
    void setAuthenticationToken( QNetworkRequest &request );

    QString mPassword;
    QString mUsername;
    QString mUrl;
    Status mStatus = Status::Disconnected;
    QByteArray mToken;
};

#endif // QFIELDCLOUDCONNECTION_H
