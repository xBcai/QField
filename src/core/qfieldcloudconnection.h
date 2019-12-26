#ifndef QFIELDCLOUDCONNECTION_H
#define QFIELDCLOUDCONNECTION_H

#include <QObject>
#include <QVariantMap>

class QNetworkReply;

class QFieldCloudConnection : public QObject
{
    Q_OBJECT

    enum class Status
    {
      Disconnected,
      LoggedIn
    };

    Q_ENUM()

  public:
    QFieldCloudConnection();

    Q_PROPERTY( QString username READ username WRITE setUsername NOTIFY usernameChanged )
    Q_PROPERTY( QString password READ password WRITE setPassword NOTIFY passwordChanged )
    Q_PROPERTY( QString url READ url WRITE setUrl NOTIFY urlChanged )
    Q_PROPERTY( Status status READ status NOTIFY statusChanged )

    QString url() const;
    void setUrl( const QString &url );

    QString username() const;
    void setUsername( const QString &username );

    QString password() const;
    void setPassword( const QString &password );

    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();

    Status status() const;

    /**
     * Sends a request with the given \a parameters to the given \a endpoint.
     *
     * If this connection is not logged in, will return nullptr.
     */
    QNetworkReply *post(const QString& endpoint, const QVariantMap& parameters = QVariantMap() );

  signals:
    void usernameChanged();
    void passwordChanged();
    void urlChanged();
    void statusChanged();
    void error();

  private:
    void setStatus(Status status);

    QString mPassword;
    QString mUsername;
    QString mUrl;
    Status mStatus = Status::Disconnected;
    QByteArray mCsrfToken;
};

#endif // QFIELDCLOUDCONNECTION_H
