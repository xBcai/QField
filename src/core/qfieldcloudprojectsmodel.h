#ifndef QFIELDCLOUDPROJECTSMODEL_H
#define QFIELDCLOUDPROJECTSMODEL_H

#include <QStandardItemModel>

class QFieldCloudConnection;

class QFieldCloudProjectsModel : public QStandardItemModel
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

  signals:
    void cloudConnectionChanged();
    void warning( const QString &message );

  private slots:
    void connectionStatusChanged();
    void projectListReceived();
    void downloadFile( const QString &owner, const QString &projectName, const QString &fileName );

  private:
    QFieldCloudConnection *mCloudConnection = nullptr;

};

Q_DECLARE_METATYPE( QFieldCloudProjectsModel::Status )

#endif // QFIELDCLOUDPROJECTSMODEL_H
