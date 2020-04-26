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


class FeatureDeltas
{
  public:
    static QString FormatVersion;

  
    FeatureDeltas( const QString &fileName );


    /**
     * Clears the deltas from memory as there are no deltas at all. Does not affect the permanent storage until `toFile()` is called.
     */
    void clear();


    /**
     * Returns deltas file name
     * 
     * @return QString file name
     */
    QString fileName();


    /**
     * Returns deltas file project id
     * 
     * @return QString project id
     */
    QString projectId();


    /**
     * Checks whether the class has encountered I/O error regarding the delta file. If true is returned, the behaviour of the class instance is no more defined.
     *
     * @return bool whether an error has been encountered
     */
    bool hasError();


    /**
     * Human readable error description why the class has an error.
     * 
     * @return QString human readable error reason
     */
    QString errorString();


    /**
     * Returns deltas as JSON QByteArray, ready for I/O operations.
     *
     * @param jsonFormat formatting of the output JSON. Default: QJsonDocument::Indented
     * @return QByteArray JSON representation
     */
    QByteArray toJson( QJsonDocument::JsonFormat jsonFormat = QJsonDocument::Indented );


    /**
     * Returns deltas as JSON string.
     *
     * @return QString JSON representation
     */
    QString toString();


    /**
     * Writes deltas file to the permanent storage.
     *
     * @return bool whether write has been successful
     */
    bool toFile();


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
    QJsonValue geometryToJsonValue( const QgsGeometry &geom );

    /**
     * Delta file format version.
     */
    QString mDeltaFileFormatVersion = "1.0";


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
     * Error string describing the reason for the error. Should be checked if hasError() returns true.
     */
    QString mErrorReason;

};

#endif // FEATUREDELTAS_H
