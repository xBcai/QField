/***************************************************************************
                        deltafilewrapper.h
                        ------------------
  begin                : Apr 2020
  copyright            : (C) 2020 by Ivan Ivanov
  email                : ivan@opengis.ch
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FEATUREDELTAS_H
#define FEATUREDELTAS_H

#include <qgsfeature.h>
#include <qgsvectorlayer.h>
#include <qgslogger.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

/**
 * A class that wraps the operations with a delta file. All read and write operations to a delta file should go through this class.
 * 
 */
class DeltaFileWrapper : public QObject
{
  Q_OBJECT

  Q_PROPERTY( int count READ count NOTIFY countChanged )
  Q_PROPERTY( const QStringList offlineLayerIds READ offlineLayerIds NOTIFY offlineLayerIdsChanged )

  public:
    /**
     * Error types
     */
    enum ErrorTypes
    {
      NoError,
      LockError,
      NotCloudProjectError,
      IOError,
      JsonParseError,
      JsonFormatIdError,
      JsonFormatProjectIdError,
      JsonFormatVersionError,
      JsonFormatDeltasError,
      JsonFormatDeltaItemError,
      JsonFormatOfflineLayersError,
      JsonFormatOfflineLayersItemError,
      JsonIncompatibleVersionError
    };


    /**
     * Stores the current version of the format.
     */
    const static QString FormatVersion;

  
    /**
     * Construct a new Feature Deltas object.
     * 
     * @param fileName complete file name with path where the object should be stored
     */
    DeltaFileWrapper( const QgsProject *project, const QString &fileName );

    /**
     * Destroy the Delta File Wrapper object
     */
    ~DeltaFileWrapper();


    /**
     * Returns a list of field names that have edit form as attachments
     * 
     * @param project current project instance
     * @param layerId layer ID
     * @return QStringList list of field names
     */
    static QStringList attachmentFieldNames( const QgsProject *project, const QString &layerId );


    /**
     * Creates checksum of a file. Returns null QByteArray if cannot be calculated.
     * 
     * @param fileName file name to get checksum of
     * @param hashAlgorithm hash algorithm (md5, sha1, sha256 etc)
     * @return QByteArray checksum
     */
    static QByteArray fileChecksum( const QString &fileName, const QCryptographicHash::Algorithm hashAlgorithm = QCryptographicHash::Sha256 );


    /**
     * Clears the deltas from memory as there are no deltas at all. Does not affect the permanent storage until `toFile()` is called.
     * @param isHardReset if true, then the delta is recreated from scratch
     */
    void reset( bool isHardReset = false );


    /**
     * Returns deltas file id.
     * 
     * @return QString id
     */
    QString id() const;


    /**
     * Returns deltas file name.
     * 
     * @return QString file name
     */
    QString fileName() const;


    /**
     * Returns deltas file project id.
     * 
     * @return QString project id
     */
    QString projectId() const;


    /**
     * Returns whether the class has encountered I/O error regarding the delta file. If true is returned, the behaviour of the class instance is no more defined.
     *
     * @return bool whether an error has been encountered
     */
    bool hasError() const;


    /**
     * Returns whether the instance contents differs from the data saved on the disk.
     *
     * @return bool whether there is a difference with the data saved on the disk
     */
    bool isDirty() const;


    /**
     * Returns the number of delta elements
     *
     * @return int number of delta elements
     */
    int count() const;


    /**
     * Returns the deltas as a JSON array of delta elements
     * 
     * @return QJsonArray deltas JSON array
     */
    QJsonArray deltas() const;


    /**
     * Error type why the class has an error.
     * 
     * @return ErrorTypes error type
     */
    ErrorTypes errorType() const;

    /**
     * Human readable error description why the class has an error.
     * 
     * @return QString human readable error reason
     */
    QString errorString() const;


    /**
     * Returns deltas as JSON QByteArray, ready for I/O operations.
     *
     * @param jsonFormat formatting of the output JSON. Default: QJsonDocument::Indented
     * @return QByteArray JSON representation
     */
    QByteArray toJson( QJsonDocument::JsonFormat jsonFormat = QJsonDocument::Indented ) const;


    /**
     * Returns deltas as JSON string.
     *
     * @return QString JSON representation
     */
    QString toString() const;


    /**
     * Writes deltas file to the permanent storage.
     *
     * @return bool whether write has been successful
     */
    bool toFile();


    /**
     * Appends the provided deltas JSON array at the end of the current file
     */
    bool append( const DeltaFileWrapper *deltaFileWrapper );


    /**
     * Returns a set of file names to be uploaded
     * 
     * @return QMap<QString, QString> unique file names
     */
    QMap<QString, QString> attachmentFileNames() const;


    /**
     * Adds create delta.
     *
     * @param layerId layer ID where the old feature belongs to
     * @param newFeature the feature that has been created
     */
    void addCreate( const QString &layerId, const QgsFeature &newFeature );


    /**
     * Adds delete delta.
     *
     * @param layerId layer ID where the old feature belongs to
     * @param oldFeature the feature that has been deleted
     */
    void addDelete( const QString &layerId, const QgsFeature &oldFeature );


    /**
     * Adds patch delta.
     *
     * @param layerId layer ID where the old feature belongs to
     * @param oldFeature the old version of the feature that has been modified
     * @param newFeature the new version of the feature that has been modified
     */
    void addPatch( const QString &layerId, const QgsFeature &oldFeature, const QgsFeature &newFeature );


    /**
     * Returns the list of offline layers to be synchronized.
     * @return list of layers
     * @todo TEST
     */
    QStringList offlineLayerIds() const;


    /**
     * Adds a new offline layer id to be syncronized.
     *
     * @param offlineLayerId target layer id
     * @todo TEST
     */
    void addOfflineLayerId( const QString &offlineLayerId );


signals:
    /**
     * Emitted when the `deltas` list has changed.
     *
     * @todo TEST
     */
    void countChanged();


    /**
     * Emitted when the `offlineLayerIds` has changed.
     *
     * @todo TEST
     */
    void offlineLayerIdsChanged();

private:

    /**
     * Converts geometry to QJsonValue string in WKT format.
     * Returns null if the geometry is null, or WKT string of the geometry
     * 
     */
    QJsonValue geometryToJsonValue( const QgsGeometry &geom ) const;


    /**
     * Attachment fields cache.
     */
    static QMap<QString, QStringList> sCacheAttachmentFieldNames;


    /**
     * Storage to keep track of the currently opened files. The stored paths are absolute, to ensure they are unique.
     */
    static QSet<QString> sFileLocks;


    /**
     * The current project instance
     */
    const QgsProject *mProject = nullptr;


    /**
     * The list of JSON deltas.
     */
    QJsonArray mDeltas;


    /**
     * The list offline layer ids to be synchronized.
     */
    QStringList mOfflineLayerIds;


    /**
     * The root deltas JSON object.
     */
    QJsonObject mJsonRoot;


    /**
     * The delta file name.
     */
    QString mFileName;
    
    
    /**
     * The delta file project id.
     */
    QString mCloudProjectId;


    /**
     * Type of error that the constructor has encountered.
     */
    ErrorTypes mErrorType = NoError;


    /**
     * Additional details describing the error.
     */
    QString mErrorDetails;


    /**
     * Holds whether the deltas in the memory differ from the deltas in the file
     */
    bool mIsDirty = false;
};

#endif // FEATUREDELTAS_H
