#ifndef QFIELDCLOUDPROJECTSMODEL_H
#define QFIELDCLOUDPROJECTSMODEL_H

#include <QAbstractListModel>

class QFieldCloudConnection;

class QFieldCloudProjectsModel : public QAbstractListModel
{
    Q_OBJECT

  public:
    enum ColumnRole
    {
      IdRole = Qt::UserRole + 1,
      OwnerRole,
      NameRole,
      DescriptionRole,
      StatusRole,
      DownloadProgressRole,
      LocalPathRole
    };

    Q_ENUM( ColumnRole )

    enum class ProjectStatus
    {
      Available,
      Downloading,
      Uploading,
      Synchronized,
      HasLocalChanges,
      HasRemoteChanges,
      LocalOnly
    };

    Q_ENUM( ProjectStatus )

    QFieldCloudProjectsModel();

    Q_PROPERTY( QFieldCloudConnection *cloudConnection READ cloudConnection WRITE setCloudConnection NOTIFY cloudConnectionChanged )

    QFieldCloudConnection *cloudConnection() const;
    void setCloudConnection( QFieldCloudConnection *cloudConnection );

    Q_INVOKABLE void refreshProjectsList();
    Q_INVOKABLE void downloadProject( const QString &projectId );
    Q_INVOKABLE void removeLocalProject( const QString &projectId );

    QHash<int, QByteArray> roleNames() const;

    int rowCount( const QModelIndex &parent ) const override;
    QVariant data( const QModelIndex &index, int role ) const override;

    Q_INVOKABLE void reload( const QJsonArray &remoteProjects );

  signals:
    void cloudConnectionChanged();
    void warning( const QString &message );
    void projectDownloaded( const QString projectId, const QString projectName, const bool failed = false );

  private slots:
    void connectionStatusChanged();
    void projectListReceived();

    void downloadFile( const QString &projectId, const QString &fileName );

    int findProject( const QString &projectId );

  private:
    struct CloudProject
    {
      CloudProject( const QString &id, const QString &owner, const QString &name, const QString &description, const ProjectStatus &status  )
        : id( id )
        , owner( owner )
        , name( name )
        , description( description )
        , status( status )
      {}

      CloudProject() = default;

      QString id;
      QString owner;
      QString name;
      QString description;
      ProjectStatus status;
      QString localPath;
      QMap<QString, int> files;
      int filesSize = 0;
      int filesFailed = 0;
      int downloadedSize = 0;
      double downloadProgress = 0.0; // range from 0.0 to 1.0
    };

    QList<CloudProject> mCloudProjects;
    QFieldCloudConnection *mCloudConnection = nullptr;

};

Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectStatus )

#endif // QFIELDCLOUDPROJECTSMODEL_H
