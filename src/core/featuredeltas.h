/***************************************************************************
                          featuredeltas.h
                             -------------------
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
class FeatureDeltas
{
  public:
    /**
     * Stores the current version of the format.
     */
    const static QString FormatVersion;

  
    /**
     * Construct a new Feature Deltas object.
     * 
     * @param fileName full file name with path where the object should be stored
     */
    FeatureDeltas( const QString &fileName );


    /**
     * Clears the deltas from memory as there are no deltas at all. Does not affect the permanent storage until `toFile()` is called.
     */
    void clear();


    /**
     * Returns deltas file name.
     * 
     * @return QString file name
     */
    QString fileName() const;


    /**
     * Returns deltas file project id
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
     * Returns the number of deltas
     *
     * @return int number of deltas
     */
    int count() const;


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
     * Returns a list of field names that have edit form as attachments
     * 
     * @param layerId
     * @return QStringList list of field names
     */
    QStringList attachmentFieldNames( const QString &layerId ) const;


    /**
     * Returns a set of file names to be uploaded
     * 
     * @return QSet<QString> unique file names
     */
    QSet<QString> attachmentFileNames() const;


    /**
     * Creates checksum of a file. Returns null QByteArray if cannot be calculated.
     * 
     * @param fileName file name to get checksum of
     * @param hashAlgorithm hash algorithm (md5, sha1, sha256 etc)
     * @return QByteArray checksum
     */
    QByteArray fileChecksum( const QString &fileName, const QCryptographicHash::Algorithm hashAlgorithm ) const;


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

  private:

    /**
     * Converts geometry to QJsonValue string in WKT format. 
     * Returns null if the geometry is null, or WKT string of the geometry
     * 
     */
    QJsonValue geometryToJsonValue( const QgsGeometry &geom ) const;

    /**
     * The list of JSON deltas.
     */
    QJsonArray mDeltas;


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
    QString mProjectId;


    /**
     * Holds whether the constructor experienced I/O error.
     */
    bool mHasError = false;


    /**
     * Holds whether the deltas in the memory differ from the deltas in the file
     */
    bool mIsDirty = false;


    /**
     * Error string describing the reason for the error. Should be checked if hasError() returns true.
     */
    QString mErrorReason;


    /**
     * Attachment fields cache.
     */
    QMap<QString, QStringList> mCacheAttachmentFieldNames;
};

#endif // FEATUREDELTAS_H
