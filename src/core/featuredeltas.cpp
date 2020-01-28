// QgsVectorLayerEditBuffer.deletedFeatureIds()
// QgsVectorLayerEditBuffer.createdFeatureIds()
// QgsVectorLayerEditBuffer.updatedFeatureIds()
#include "featuredeltas.h"

#include <QFileInfo>
#include <QFile>
#include <QUuid>

#include <qgsproject.h>


FeatureDeltas::FeatureDeltas( const QString &fileName )
{
  mFileName = fileName;

  if ( QFileInfo::exists( mFileName ) )
  {
    QFile deltasFile( mFileName );
    QgsLogger::debug( QStringLiteral( "Loading deltas from file %1" ).arg( mFileName ) );

    if ( ! deltasFile.open( QIODevice::ReadWrite ) )
    {
      QgsLogger::warning( QStringLiteral( "Cannot open file for read and write" ) );
      mHasError = true;
      return;
    }

    mJsonRoot = QJsonDocument::fromJson( deltasFile.readAll() ).object();
    
    deltasFile.close();

    if ( ! mJsonRoot.value( "version" ).isString()
      || ! mJsonRoot.value( "id" ).isString()
      || ! mJsonRoot.value( "projectId" ).isString()
      || ! mJsonRoot.value( "timestamp" ).isString()
      || ! mJsonRoot.value( "deltas" ).isArray() )
    {
      QgsLogger::warning( QStringLiteral( "File contains invalid JSON" ) );
      mHasError = true;
      return;
    }

    if ( mJsonRoot.value( "version" ) != mDeltaFileVersion )
    {
      QgsLogger::warning( QStringLiteral( "File has incompatible version" ) );
      mHasError = true;
      return;
    }

    for ( const QJsonValue &v : mJsonRoot.value( "deltas" ).toArray() )
    {
      // ? how far should I go in checking the validity?
      if ( ! v.isObject() )
        QgsLogger::warning( QStringLiteral( "Encountered delta element that is not an object" ) );

      mDeltas.append( v );
    }
  }
  else
  {
    QString projectId = QgsProject::instance()->baseName();
    QString timestamp = QDateTime::currentDateTime().toUTC().toString( Qt::ISODateWithMs );

    mJsonRoot = QJsonObject( {{"version", mDeltaFileVersion},
                              {"id", QUuid::createUuid().toString( QUuid::WithoutBraces )},
                              // Mario thinks this is not needed for now
                              // {"clientId",clientId},
                              {"projectId", projectId},
                              {"timestamp", timestamp},
                              // It seems this should go to individual deltas
                              // {"layerId",layer.id()},
                              {"deltas", mDeltas}} );
  }
}


bool FeatureDeltas::hasError()
{
  return mHasError;
}


QByteArray FeatureDeltas::toJson( QJsonDocument::JsonFormat jsonFormat )
{
  mJsonRoot.insert( QStringLiteral( "deltas" ), mDeltas );

  return QJsonDocument( mJsonRoot ).toJson( jsonFormat );
}


QString FeatureDeltas::toString()
{
  return QString::fromStdString( toJson().toStdString() );
}


bool FeatureDeltas::toFile()
{
  QFile deltasFile( mFileName );

  QgsLogger::warning( "Start writing deltas JSON: " + toString() );

  if ( ! deltasFile.open( QIODevice::WriteOnly | QIODevice::Unbuffered ) )
  {
    QgsLogger::warning( QStringLiteral( "File %1 cannot be open for writing. Reason %2" ).arg( mFileName ).arg( deltasFile.errorString() ) );

    return false;
  }

  if ( deltasFile.write( toJson() )  == -1)
  {
    QgsLogger::warning( QStringLiteral( "Contents of the file %1 has not been written. Reason %2" ).arg( mFileName ).arg( deltasFile.errorString() ) );
    return false;
  }

  deltasFile.close();

  return true;
}


void FeatureDeltas::addPatch( const QgsVectorLayer *layer, const QgsFeature &oldFeature, const QgsFeature &newFeature )
{
  QJsonObject delta( {
    {"fid", oldFeature.id()},
    {"layerId", layer->id()}
  } );
  QgsGeometry oldGeom = oldFeature.geometry();
  QgsGeometry newGeom = newFeature.geometry();
  QgsAttributes oldAttrs = oldFeature.attributes();
  QgsAttributes newAttrs = newFeature.attributes();
  QJsonObject oldData;
  QJsonObject newData;

  if ( !oldGeom.equals( newGeom ) )
  {
    oldData.insert( QStringLiteral( "geometry" ), oldGeom.isNull() ? QJsonValue::Null : QJsonValue( oldGeom.asWkt() ) );
    newData.insert( QStringLiteral( "geometry" ), newGeom.isNull() ? QJsonValue::Null : QJsonValue( newGeom.asWkt() ) );
  }

  QgsFields layerFields = layer->fields();
  QJsonObject tmpOldAttrs;
  QJsonObject tmpNewAttrs;

  Q_ASSERT( layerFields == oldFeature.fields() && layerFields == newFeature.fields() );

  for ( int idx = 0; idx < layerFields.count(); ++idx )
  {
    QVariant oldVal = oldAttrs.at( idx );
    QVariant newVal = newAttrs.at( idx );

    if ( newVal != oldVal )
    {
      QString name = layerFields.at( idx ).name();
      tmpOldAttrs.insert( name, QJsonValue::fromVariant( oldVal ) );
      tmpNewAttrs.insert( name, QJsonValue::fromVariant( newVal ) );
    }
  }

  if ( tmpNewAttrs.length() > 0 || tmpOldAttrs.length() > 0 )
  {
    oldData.insert( QStringLiteral( "attributes" ), tmpOldAttrs );
    newData.insert( QStringLiteral( "attributes" ), tmpNewAttrs );
  }

  delta.insert( QStringLiteral( "old" ), oldData );
  delta.insert( QStringLiteral( "new" ), newData );

  mDeltas.append( delta );
}


void FeatureDeltas::addDelete( const QgsVectorLayer *layer, const QgsFeature &oldFeature )
{
  QJsonObject delta( {{"fid", oldFeature.id()},
                      {"layerId", layer->id()}} );
  QgsGeometry oldGeom = oldFeature.geometry();
  QgsAttributes oldAttrs = oldFeature.attributes();
  QJsonObject oldData( {{"geometry", oldGeom.isNull() ? QJsonValue::Null : QJsonValue( oldGeom.asWkt() )}});
  QJsonObject tmpOldAttrs;

  for ( int idx = 0; idx < oldAttrs.count(); ++idx )
  {
    QVariant oldVal = oldAttrs.at( idx );
    QString name = layer->fields().at( idx ).name();
    tmpOldAttrs.insert( name, QJsonValue::fromVariant( oldVal ) );
  }

  oldData.insert( QStringLiteral( "attributes" ), tmpOldAttrs );
  delta.insert( QStringLiteral( "old" ), oldData );

  mDeltas.append( delta );
}


void FeatureDeltas::addCreate( const QgsVectorLayer *layer, const QgsFeature &newFeature )
{
  QJsonObject delta( {{"fid", newFeature.id()},
                      {"layerId", layer->id()}} );
  QgsGeometry newGeom = newFeature.geometry();
  QgsAttributes newAttrs = newFeature.attributes();

  QJsonObject newData( {{"geometry", newGeom.isNull() ? QJsonValue::Null : QJsonValue( newGeom.asWkt() )}} );
  QJsonObject tmpNewAttrs;

  for ( int idx = 0; idx < newAttrs.count(); ++idx )
  {
    QVariant newVal = newAttrs.at( idx );
    QString name = layer->fields().at( idx ).name();
    tmpNewAttrs.insert( name, QJsonValue::fromVariant( newVal ) );
  }


  newData.insert( QStringLiteral( "attributes" ), tmpNewAttrs );
  delta.insert( QStringLiteral( "new" ), newData );

  mDeltas.append( delta );
}
