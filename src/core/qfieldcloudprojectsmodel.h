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


class QNetworkRequest;
class QFieldCloudConnection;
class NetworkReply;
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
      ErrorStatusRole,
      ErrorStringRole,
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
    };

    Q_ENUM( ProjectStatus )

    enum ProjectErrorStatus
    {
      NoErrorStatus,
      DownloadErrorStatus,
      UploadErrorStatus,
    };

    Q_ENUM( ProjectErrorStatus )

    enum ProjectCheckout
    {
      RemoteCheckout = 2 << 0,
      LocalCheckout = 2 << 1,
      LocalAndRemoteCheckout = RemoteCheckout | LocalCheckout
    };

    Q_ENUM( ProjectCheckout )
    Q_DECLARE_FLAGS( ProjectCheckouts, ProjectCheckout )
    Q_FLAG( ProjectCheckouts )

    enum ProjectModification
    {
      NoModification = 0,
      LocalModification = 2 << 0,
      RemoteModification = 2 << 1,
      LocalAndRemoteModification = RemoteModification | LocalModification
    };

    Q_ENUM( ProjectModification )
    Q_DECLARE_FLAGS( ProjectModifications, ProjectModification )
    Q_FLAG( ProjectModifications )

    enum class LayerAction
    {
      Offline,
      NoAction,
      Remove,
      Cloud,
      Unknown
    };

    Q_ENUM( LayerAction )

    enum DeltaFileStatus
    {
      DeltaFileErrorStatus,
      DeltaFileLocalStatus,
      DeltaFilePendingStatus,
      DeltaFileWaitingStatus,
      DeltaFileBusyStatus,
      DeltaFileAppliedStatus,
      DeltaFileAppliedWithConflictsStatus
    };

    Q_ENUM( DeltaFileStatus )

    enum DownloadJobStatus
    {
      DownloadJobErrorStatus,
      DownloadJobUnstartedStatus,
      DownloadJobPendingStatus,
      DownloadJobQueuedStatus,
      DownloadJobStartedStatus,
      DownloadJobCreatedStatus
    };

    Q_ENUM( DownloadJobStatus )

    QFieldCloudProjectsModel();

    Q_PROPERTY( QFieldCloudConnection *cloudConnection READ cloudConnection WRITE setCloudConnection NOTIFY cloudConnectionChanged )

    QFieldCloudConnection *cloudConnection() const;
    void setCloudConnection( QFieldCloudConnection *cloudConnection );

    Q_PROPERTY( LayerObserver *layerObserver READ layerObserver WRITE setLayerObserver NOTIFY layerObserverChanged )

    LayerObserver *layerObserver() const;
    void setLayerObserver( LayerObserver *layerObserver );

    Q_PROPERTY( QString currentProjectId READ currentProjectId WRITE setCurrentProjectId NOTIFY currentProjectIdChanged )
    Q_PROPERTY( ProjectStatus currentProjectStatus READ currentProjectStatus NOTIFY currentProjectStatusChanged )
    Q_PROPERTY( bool canCommitCurrentProject READ canCommitCurrentProject NOTIFY canCommitCurrentProjectChanged )
    Q_PROPERTY( bool canSyncCurrentProject READ canSyncCurrentProject NOTIFY canSyncCurrentProjectChanged )

    QString currentProjectId() const;
    void setCurrentProjectId( const QString &currentProjectId );

    ProjectStatus currentProjectStatus() const;

    Q_INVOKABLE void refreshProjectsList();
    Q_INVOKABLE void downloadProject( const QString &projectId );
    Q_INVOKABLE void uploadProject( const QString &projectId, const bool shouldDownloadUpdates );
    Q_INVOKABLE void removeLocalProject( const QString &projectId );

    /**
     * Reverts the changes of the current cloud project.
     */
    Q_INVOKABLE bool discardLocalChangesFromCurrentProject();
    Q_INVOKABLE ProjectStatus projectStatus( const QString &projectId );
    Q_INVOKABLE ProjectModifications projectModification( const QString &projectId ) const;
    Q_INVOKABLE void refreshProjectModification( const QString &projectId );

    QHash<int, QByteArray> roleNames() const;

    int rowCount( const QModelIndex &parent ) const override;
    QVariant data( const QModelIndex &index, int role ) const override;

    Q_INVOKABLE void reload( const QJsonArray &remoteProjects );

  signals:
    void cloudConnectionChanged();
    void layerObserverChanged();
    void currentProjectIdChanged();
    void currentProjectStatusChanged();
    void canCommitCurrentProjectChanged();
    void canSyncCurrentProjectChanged();
    void warning( const QString &message );
    void projectDownloaded( const QString &projectId, const bool hasError, const QString &projectName );
    void projectStatusChanged( const QString &projectId, const ProjectStatus &projectStatus );

    //
    void networkDeltaUploaded( const QString &projectId );
    void networkDeltaStatusChecked( const QString &projectId );
    void networkDownloadStatusChecked( const QString &projectId );
    void networkAttachmentsUploaded( const QString &projectId );
    void networkAllAttachmentsUploaded( const QString &projectId );
    void networkLayerDownloaded( const QString &projectId );
    void networkAllLayersDownloaded( const QString &projectId );
    void syncFinished( const QString &projectId, bool hasError, const QString &errorString = QString() );
    void downloadFinished( const QString &projectId, bool hasError, const QString &errorString = QString() );

  private slots:
    void connectionStatusChanged();
    void projectListReceived();

    NetworkReply *uploadFile( const QString &projectId, const QString &fileName );

    int findProject( const QString &projectId ) const;

    void layerObserverLayerEdited( const QString &layerId );
  private:
    static const int sDelayBeforeStatusRetry = 1000;

    struct FileTransfer
    {
      FileTransfer(
        const QString &fileName,
        const int bytesTotal,
        NetworkReply *networkReply = nullptr,
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
      NetworkReply *networkReply;
      QNetworkReply::NetworkError error = QNetworkReply::NoError;
      QStringList layerIds;
    };

    struct CloudProject
    {
      CloudProject( const QString &id, const QString &owner, const QString &name, const QString &description, const QString &updatedAt, const ProjectCheckouts &checkout, const ProjectStatus &status )
        : id( id )
        , owner( owner )
        , name( name )
        , description( description )
        , updatedAt( updatedAt )
        , status( status )
        , checkout( checkout )
      {}

      CloudProject() = default;

      QString id;
      QString owner;
      QString name;
      QString description;
      QString updatedAt;
      ProjectStatus status;
      ProjectErrorStatus errorStatus = ProjectErrorStatus::NoErrorStatus;
      ProjectCheckouts checkout;
      ProjectModifications modification = ProjectModification::NoModification;
      QString localPath;

      QString deltaFileId;
      DeltaFileStatus deltaFileUploadStatus = DeltaFileLocalStatus;
      QString deltaFileUploadStatusString;
      QStringList deltaLayersToDownload;

      QString downloadJobId;
      DownloadJobStatus downloadJobStatus = DownloadJobUnstartedStatus;
      QString downloadJobStatusString;
      QMap<QString, FileTransfer> downloadFileTransfers;
      int downloadFilesFinished = 0;
      int downloadFilesFailed = 0;
      int downloadBytesTotal = 0;
      int downloadBytesReceived = 0;
      double downloadProgress = 0.0; // range from 0.0 to 1.0

      QMap<QString, FileTransfer> uploadAttachments;
      int uploadAttachmentsFinished = 0;
      int uploadAttachmentsFailed = 0;
      int uploadAttachmentsBytesTotal = 0;

      QMap<QString, FileTransfer> downloadLayers;
      int downloadLayersFinished = 0;
      int downloadLayersFailed = 0;
      int downloadLayersBytesTotal = 0;
    };

    inline QString layerFileName( const QgsMapLayer *layer ) const;

    QList<CloudProject> mCloudProjects;
    QFieldCloudConnection *mCloudConnection = nullptr;
    QString mCurrentProjectId;
    LayerObserver *mLayerObserver = nullptr;

    bool mCanCommitCurrentProject = false;
    bool mCanSyncCurrentProject = false;

    void projectCancelUpload( const QString &projectId );
    void projectUploadAttachments( const QString &projectId );
    void projectGetDeltaStatus( const QString &projectId );
    void projectGetDownloadStatus( const QString &projectId );
    void projectDownloadLayers( const QString &projectId );

    NetworkReply *downloadFile( const QString &exportJobId, const QString &fileName );
    void projectDownloadFiles( const QString &projectId );

    bool canCommitCurrentProject();
    bool canSyncCurrentProject();
    void updateCanCommitCurrentProject();
    void updateCanSyncCurrentProject();
};

Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectStatus )
Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectCheckout )
Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectCheckouts )
Q_DECLARE_OPERATORS_FOR_FLAGS( QFieldCloudProjectsModel::ProjectCheckouts )
Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectModification )
Q_DECLARE_METATYPE( QFieldCloudProjectsModel::ProjectModifications )
Q_DECLARE_OPERATORS_FOR_FLAGS( QFieldCloudProjectsModel::ProjectModifications )
Q_DECLARE_METATYPE( QFieldCloudProjectsModel::LayerAction )

#endif // QFIELDCLOUDPROJECTSMODEL_H
