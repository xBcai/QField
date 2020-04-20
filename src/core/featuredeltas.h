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
    FeatureDeltas( const QString &filename );

    bool hasError();
    QByteArray toJson( QJsonDocument::JsonFormat jsonFormat = QJsonDocument::Indented );
    QString toString();
    bool toFile();

    void addCreate( const QString &layerId, const QgsFeature &oldFeature );
    void addDelete( const QString &layerId, const QgsFeature &newFeature );
    void addPatch( const QString &layerId, const QgsFeature &oldFeature, const QgsFeature &newFeature );

  private:
    QString mDeltaFileVersion = "1.0";
    QJsonArray mDeltas;
    QJsonObject mJsonRoot;
    QString mFileName;
    bool mHasError = false;
};

#endif // FEATUREDELTAS_H
