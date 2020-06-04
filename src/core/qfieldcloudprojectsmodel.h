/***************************************************************************
    qfieldcloudprojectsmodel.h
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

#ifndef QFIELDCLOUDPROJECTSMODEL_H
#define QFIELDCLOUDPROJECTSMODEL_H

#include "qgsnetworkaccessmanager.h"

#include <QAbstractListModel>
#include <QNetworkReply>
#include <QTimer>
#include <QRandomGenerator>

class QNetworkRequest;
class QFieldCloudConnection;
class QfNetworkReply;
class LayerObserver;
class QgsMapLayer;


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
      ModificationRole,
      CheckoutRole,
      StatusRole,
      DownloadProgressRole,
      UploadProgressRole,
      LocalPathRole
    };

    Q_ENUM( ColumnRole )

    enum class ProjectStatus
    {
      Idle,
      Downloading,
      Uploading,
      Error
    };

    Q_ENUM( ProjectStatus )

    enum ProjectCheckout
    {
      RemoteCheckout = 2 << 0,
      LocalCheckout = 2 << 1,
      LocalFromRemoteCheckout = RemoteCheckout | LocalCheckout
    };

    Q_ENUM( ProjectCheckout )
    Q_DECLARE_FLAGS( ProjectCheckouts, ProjectCheckout )

    enum ProjectModification
    {
      NoModification = 0,
      LocalModification = 2 << 0,
      RemoteModification = 2 << 1,
      LocalAndRemoteModification = RemoteModification | LocalModification
    };

    Q_ENUM( ProjectModification )
    Q_DECLARE_FLAGS( ProjectModifications, ProjectModification )

    enum class LayerAction
    {
      Offline,
      NoAction,
      Remove,
      Cloud,
      Unknown
    };

    Q_ENUM( LayerAction )

    enum class DeltaFileStatus
    {
      Error,
      Local,
      Pending,
      Waiting,
      Busy,
      Applied,
      AppliedWithConflicts
    };

    Q_ENUM( DeltaFileStatus )

    QFieldCloudProjectsModel();

    Q_PROPERTY( QFieldCloudConnection *cloudConnection READ cloudConnection WRITE setCloudConnection NOTIFY cloudConnectionChanged )

    QFieldCloudConnection *cloudConnection() const;
    void setCloudConnection( QFieldCloudConnection *cloudConnection );

    Q_PROPERTY( LayerObserver *layerObserver READ layerObserver WRITE setLayerObserver NOTIFY layerObserverChanged )

    LayerObserver *layerObserver() const;
    void setLayerObserver( LayerObserver *layerObserver );

    Q_PROPERTY( QString currentCloudProjectId READ currentCloudProjectId WRITE setCurrentCloudProjectId NOTIFY currentCloudProjectIdChanged )

    QString currentCloudProjectId() const;
    void setCurrentCloudProjectId( const QString &currentCloudProjectId );

    Q_INVOKABLE void refreshProjectsList();
    Q_INVOKABLE void downloadProject( const QString &projectId );
    Q_INVOKABLE void uploadProject( const QString &projectId );
    Q_INVOKABLE void removeLocalProject( const QString &projectId );
    Q_INVOKABLE ProjectStatus projectStatus( const QString &projectId );
    Q_INVOKABLE ProjectModifications projectModification( const QString &projectId ) const;
    void projectCancelProjectUpload( const QString &projectId, bool shouldCancelAtServer );

    QHash<int, QByteArray> roleNames() const;

    int rowCount( const QModelIndex &parent ) const override;
    QVariant data( const QModelIndex &index, int role ) const override;

    Q_INVOKABLE void reload( const QJsonArray &remoteProjects );

  signals:
    void cloudConnectionChanged();
    void layerObserverChanged();
    void currentCloudProjectIdChanged();
    void warning( const QString &message );
    void projectDownloaded( const QString projectId, const QString projectName, const bool failed = false );
    void projectStatusChanged( const QString projectId, const ProjectStatus projectStatus );

    //
    void networkDeltaUploaded( const QString &projectId );
    void networkOfflineLayersUploaded( const QString &projectId );
    void networkDeltaStatusChecked( const QString &projectId );
    void networkAttachmentsUploaded( const QString &projectId );
    void networkLayerDownloaded( const QString &projectId );
    void syncFailed( const QString &projectId, const QString &reason );

  private slots:
    void connectionStatusChanged();
    void projectListReceived();

    void downloadFile( const QString &projectId, const QString &fileName );
    QfNetworkReply *uploadFile( const QString &projectId, const QString &fileName );

    int findProject( const QString &projectId ) const;

    void layerObserverLayerEdited( const QString &layerId );
  private:
    static const int sDelayBeforeDeltaStatusRetry = 1000;

    struct FileTransfer
    {
      FileTransfer(
        const QString &fileName,
        const int bytesTotal,
        QfNetworkReply *networkReply = nullptr,
        const QStringList &layerIds = QStringList()
      )
        : fileName( fileName ),
          bytesTotal( bytesTotal ),
          networkReply( networkReply ),
          layerIds( layerIds )
      {};

      FileTransfer() = default;

      QString fileName;
      int bytesTotal;
      int bytesTransferred = 0;
      bool isFinished = false;
      QfNetworkReply *networkReply;
      QNetworkReply::NetworkError error = QNetworkReply::NoError;
      QStringList layerIds;
    };

    struct CloudProject
    {
      CloudProject( const QString &id, const QString &owner, const QString &name, const QString &description, const ProjectCheckouts &checkout, const ProjectStatus &status )
        : id( id )
        , owner( owner )
        , name( name )
        , description( description )
        , status( status )
        , checkout( checkout )
      {}

      CloudProject() = default;

      QString id;
      QString owner;
      QString name;
      QString description;
      ProjectStatus status;
      ProjectCheckouts checkout;
      ProjectModifications modification = ProjectModification::NoModification;
      QString localPath;

      QString deltaFileId;
      DeltaFileStatus deltaFileStatus = DeltaFileStatus::Local;
      QStringList deltaLayersToDownload;

      int layersDownloadedFinished = 0;
      int layersDownloadedFailed = 0;

      QMap<QString, int> files;
      int filesSize = 0;
      int filesFailed = 0;
      int downloadedSize = 0;
      double downloadProgress = 0.0; // range from 0.0 to 1.0

      // TODO it would be nice to have some `UploadFile` structure in the future, instead of QVariantMap
      QMap<QString, FileTransfer> offlineLayers;
      int offlineLayersFinished = 0;
      int offlineLayersFailed = 0;

      QMap<QString, FileTransfer> attachments;
      int attachmentsFinished = 0;
      int attachmentsFailed = 0;

      int uploadFileSize = 0;
      int uploadedSize = 0;
      double uploadProgress = 0.0; // range from 0.0 to 1.0

    };

    inline QString layerFileName( const QgsMapLayer *layer ) const;

    QList<CloudProject> mCloudProjects;
    QFieldCloudConnection *mCloudConnection = nullptr;
    QString mCurrentCloudProjectId;
    LayerObserver *mLayerObserver = nullptr;

    void projectCancelUpload( const QString &projectId, bool shouldCancelAtServer );
    void projectUploadOfflineLayers( const QString &projectId );
    void projectUploadAttachments( const QString &projectId );
    void projectGetDeltaStatus( const QString &projectId );
    void projectDownloadLayers( const QString &projectId );
};

Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectStatus )
Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectCheckout )
Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectModification )

#endif // QFIELDCLOUDPROJECTSMODEL_H
