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
      LocalPathRole
    };

    enum class Status
    {
      Available,
      Downloading,
      Uploading,
      Synchronized,
      HasLocalChanges,
      HasRemoteChanges,
    };

    Q_ENUMS( Status )

    QFieldCloudProjectsModel();

    Q_PROPERTY( QFieldCloudConnection *cloudConnection READ cloudConnection WRITE setCloudConnection NOTIFY cloudConnectionChanged )

    QFieldCloudConnection *cloudConnection() const;
    void setCloudConnection( QFieldCloudConnection *cloudConnection );

    Q_INVOKABLE void refreshProjectsList();
    Q_INVOKABLE void download( const QString &owner, const QString &projectName );

    QHash<int, QByteArray> roleNames() const;

    int rowCount( const QModelIndex &parent ) const override;
    QVariant data( const QModelIndex &index, int role ) const override;

    Q_INVOKABLE void reload( QJsonArray &remoteProjects );

  signals:
    void cloudConnectionChanged();
    void warning( const QString &message );

  private slots:
    void connectionStatusChanged();
    void projectListReceived();

    const QString localCloudDirectory();

    void downloadFile( const QString &owner, const QString &projectName, const QString &fileName );

  private:
    struct CloudProject
    {
      CloudProject( const QString &id, const QString &owner, const QString &name, const QString &description, const Status &status  )
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
      Status status;
      QString localPath;
    };

    QList<CloudProject> mCloudProjects;
    QFieldCloudConnection *mCloudConnection = nullptr;

};

Q_DECLARE_METATYPE( QFieldCloudProjectsModel::Status )

#endif // QFIELDCLOUDPROJECTSMODEL_H
