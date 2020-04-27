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

#include "featuredeltas.h"

#include <QFileInfo>
#include <QFile>
#include <QUuid>

#include <qgsproject.h>


const QString FeatureDeltas::FormatVersion = QStringLiteral( "1.0" );


FeatureDeltas::FeatureDeltas( const QString &fileName )
{
  mFileName = fileName;
  QFile deltasFile( mFileName );
  QString errorReason;

  // TODO fix this
  mProjectId = QStringLiteral( "<MISSING>" );

  if ( QFileInfo::exists( mFileName ) )
  {
    QJsonParseError jsonError;

    QgsLogger::debug( QStringLiteral( "Loading deltas from file %1" ).arg( mFileName ) );

    if ( ! deltasFile.open( QIODevice::ReadWrite ) )
      errorReason = QStringLiteral( "Cannot open file for read and write" );

    if ( errorReason.isEmpty() )
      mJsonRoot = QJsonDocument::fromJson( deltasFile.readAll(), &jsonError ).object();

    if ( errorReason.isEmpty() && ( jsonError.error != QJsonParseError::NoError ) )
      errorReason = jsonError.errorString();

    if ( errorReason.isEmpty() && ( ! mJsonRoot.value( "id" ).isString() || mJsonRoot.value( "id" ).toString().isEmpty() ) )
      errorReason = QStringLiteral( "Delta file is missing a valid id" );

    if ( errorReason.isEmpty() && ( ! mJsonRoot.value( "projectId" ).isString() || mJsonRoot.value( "projectId" ).toString().isEmpty() ) )
      errorReason = QStringLiteral( "Delta file is missing a valid project id" );

    if ( errorReason.isEmpty() && ! mJsonRoot.value( "deltas" ).isArray() )
      errorReason = QStringLiteral( "Delta file is missing a valid array of deltas" );

    if ( errorReason.isEmpty() && ( ! mJsonRoot.value( "version" ).isString() || mJsonRoot.value( "version" ).toString().isEmpty() ) )
      errorReason = QStringLiteral( "Delta file is missing a valid version" );

    if ( errorReason.isEmpty() && mJsonRoot.value( "version" ) != FeatureDeltas::FormatVersion )
      errorReason = QStringLiteral( "File has incompatible version" );

    if ( errorReason.isEmpty() )
    {
      for ( const QJsonValue &v : mJsonRoot.value( "deltas" ).toArray() )
      {
        // ? how far should I go in checking the validity?
        if ( ! v.isObject() )
          QgsLogger::warning( QStringLiteral( "Encountered delta element that is not an object" ) );

        mDeltas.append( v );
      }
    }
  }
  else
  {
    mJsonRoot = QJsonObject( {{"version", FeatureDeltas::FormatVersion},
                              {"id", QUuid::createUuid().toString( QUuid::WithoutBraces )},
                              // Mario thinks this is not needed for now
                              // {"clientId",clientId},
                              {"projectId", mProjectId},
                              // It seems this should go to individual deltas
                              // {"layerId",layer.id()},
                              {"deltas", mDeltas}} );

    if ( ! deltasFile.open( QIODevice::ReadWrite ) )
      errorReason = QStringLiteral( "Cannot open deltas file for read and write" );

    if ( ! toFile() )
      errorReason = QStringLiteral( "Cannot write deltas file" );
  }

  if ( ! errorReason.isEmpty() )
  {
    mErrorReason = errorReason;
    mHasError = true;
    return;
  }
}


QString FeatureDeltas::fileName() const
{
  return mFileName;
}


QString FeatureDeltas::projectId() const
{
  return mProjectId;
}


void FeatureDeltas::clear()
{
  mDeltas = QJsonArray();
}


bool FeatureDeltas::hasError() const
{
  return mHasError;
}


QString FeatureDeltas::errorString() const
{
  return mErrorReason;
}


QByteArray FeatureDeltas::toJson( QJsonDocument::JsonFormat jsonFormat ) const
{
  QJsonObject jsonRoot (mJsonRoot);
  jsonRoot.insert( QStringLiteral( "deltas" ), mDeltas );

  return QJsonDocument( jsonRoot ).toJson( jsonFormat );
}


QString FeatureDeltas::toString() const
{
  return QString::fromStdString( toJson().toStdString() );
}


bool FeatureDeltas::toFile()
{
  QFile deltasFile( mFileName );

  QgsLogger::debug( "Start writing deltas JSON" );

  if ( ! deltasFile.open( QIODevice::WriteOnly | QIODevice::Unbuffered ) )
  {
    mErrorReason = deltasFile.errorString();
    QgsLogger::warning( QStringLiteral( "File %1 cannot be open for writing. Reason %2" ).arg( mFileName ).arg( mErrorReason ) );

    return false;
  }

  if ( deltasFile.write( toJson() )  == -1)
  {
    mErrorReason = deltasFile.errorString();
    QgsLogger::warning( QStringLiteral( "Contents of the file %1 has not been written. Reason %2" ).arg( mFileName ).arg( mErrorReason ) );
    return false;
  }

  deltasFile.close();

  return true;
}


void FeatureDeltas::addPatch( const QString &layerId, const QgsFeature &oldFeature, const QgsFeature &newFeature )
{
  QJsonObject delta( {
    {"fid", oldFeature.id()},
    {"layerId", layerId},
    {"method", "patch"}
  } );
  QgsGeometry oldGeom = oldFeature.geometry();
  QgsGeometry newGeom = newFeature.geometry();
  QgsAttributes oldAttrs = oldFeature.attributes();
  QgsAttributes newAttrs = newFeature.attributes();
  QJsonObject oldData;
  QJsonObject newData;
  bool areFeaturesEqual = false;

  if ( ! oldGeom.equals( newGeom ) )
  {
    oldData.insert( QStringLiteral( "geometry" ), geometryToJsonValue( oldGeom ) );
    newData.insert( QStringLiteral( "geometry" ), geometryToJsonValue( newGeom ) );
    areFeaturesEqual = true;
  }

  Q_ASSERT( oldFeature.fields() == newFeature.fields() );

  QgsFields fields = newFeature.fields();
  QJsonObject tmpOldAttrs;
  QJsonObject tmpNewAttrs;

  for ( int idx = 0; idx < fields.count(); ++idx )
  {
    QVariant oldVal = oldAttrs.at( idx );
    QVariant newVal = newAttrs.at( idx );

    if ( newVal != oldVal )
    {
      QString name = fields.at( idx ).name();
      tmpOldAttrs.insert( name, QJsonValue::fromVariant( oldVal ) );
      tmpNewAttrs.insert( name, QJsonValue::fromVariant( newVal ) );
      areFeaturesEqual = true;
    }
  }

  // if features are completely equal, there is no need to change the JSON
  if ( ! areFeaturesEqual )
    return;

  if ( tmpNewAttrs.length() > 0 || tmpOldAttrs.length() > 0 )
  {
    oldData.insert( QStringLiteral( "attributes" ), tmpOldAttrs );
    newData.insert( QStringLiteral( "attributes" ), tmpNewAttrs );
  }

  delta.insert( QStringLiteral( "old" ), oldData );
  delta.insert( QStringLiteral( "new" ), newData );

  mDeltas.append( delta );
}


void FeatureDeltas::addDelete( const QString &layerId, const QgsFeature &oldFeature )
{
  QJsonObject delta( {{"fid", oldFeature.id()},
                      {"layerId", layerId},
                      {"method", "delete"}} );
  QgsAttributes oldAttrs = oldFeature.attributes();
  QJsonObject oldData( {{"geometry", geometryToJsonValue( oldFeature.geometry() )}});
  QJsonObject tmpOldAttrs;

  for ( int idx = 0; idx < oldAttrs.count(); ++idx )
  {
    QVariant oldVal = oldAttrs.at( idx );
    QString name = oldFeature.fields().at( idx ).name();
    tmpOldAttrs.insert( name, QJsonValue::fromVariant( oldVal ) );
  }

  if ( tmpOldAttrs.length() > 0 )
    oldData.insert( QStringLiteral( "attributes" ), tmpOldAttrs );

  delta.insert( QStringLiteral( "old" ), oldData );

  mDeltas.append( delta );
}


void FeatureDeltas::addCreate( const QString &layerId, const QgsFeature &newFeature )
{
  QJsonObject delta( {{"fid", newFeature.id()},
                      {"layerId", layerId},
                      {"method", "create"}} );
  QgsAttributes newAttrs = newFeature.attributes();

  QJsonObject newData( {{"geometry", geometryToJsonValue( newFeature.geometry() )}});
  QJsonObject tmpNewAttrs;

  for ( int idx = 0; idx < newAttrs.count(); ++idx )
  {
    QVariant newVal = newAttrs.at( idx );
    QString name = newFeature.fields().at( idx ).name();
    tmpNewAttrs.insert( name, QJsonValue::fromVariant( newVal ) );
  }

  if ( tmpNewAttrs.length() > 0 )
    newData.insert( QStringLiteral( "attributes" ), tmpNewAttrs );

  delta.insert( QStringLiteral( "new" ), newData );

  mDeltas.append( delta );
}


QJsonValue FeatureDeltas::geometryToJsonValue( const QgsGeometry &geom ) const
{
  if ( geom.isNull() )
    return QJsonValue::Null;

  return QJsonValue( geom.asWkt() );
}