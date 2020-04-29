/***************************************************************************
              featuredeltas.h
               -------------------
  begin        : Apr 2020
  copyright      : (C) 2020 by Ivan Ivanov
  email        : ivan@opengis.ch
***************************************************************************/

/***************************************************************************
 *                                     *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or   *
 *   (at your option) any later version.                   *
 *                                     *
 ***************************************************************************/

#include "layerobserver.h"
#include "qfieldcloudutils.h"

#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsvectorlayereditbuffer.h>

#include <QDir>

int LayerObserver::sFilesSaved = 0;


LayerObserver::LayerObserver( const QgsProject *project )
{
  mFeatureDeltas.reset( new FeatureDeltas( generateFileName( true ) ) );

  connect( project, &QgsProject::homePathChanged, this, &LayerObserver::onHomePathChanged );
  connect( project, &QgsProject::layersAdded, this, &LayerObserver::onLayersAdded );
}


QString LayerObserver::generateFileName( bool isCurrentDeltaFile )
{
  return ( isCurrentDeltaFile )
    ? QStringLiteral( "%1/deltafile.json" )
        .arg( QgsProject::instance()->homePath() )
    : QStringLiteral( "%1/deltafile_%2_%3.json" )
        .arg( QgsProject::instance()->homePath() )
        .arg( QDateTime::currentDateTime().toSecsSinceEpoch() )
        .arg( ++sFilesSaved );
}


QFileInfoList LayerObserver::projectDeltaFiles()
{
  return QDir( QgsProject::instance()->homePath() )
    .entryInfoList(QStringList({"deltafile_*.json"}), QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
}


QString LayerObserver::fileName() const
{
  return mFeatureDeltas->fileName();
}


bool LayerObserver::hasError() const
{
  return mFeatureDeltas->hasError();
}


bool LayerObserver::commit()
{
  if ( ! mFeatureDeltas->toFile() )
    return false;

  if ( mFeatureDeltas->count() == 0 )
    return true;

  if ( ! QDir().rename( mFeatureDeltas->fileName(), generateFileName() ) )
    return false;

  mFeatureDeltas.reset( new FeatureDeltas( generateFileName( true ) ) );

  return true;
}


void LayerObserver::clear() const
{
  return mFeatureDeltas->clear();
}


void LayerObserver::onHomePathChanged()
{
  QFileInfoList deltaFilesInfo = projectDeltaFiles();
  QString deltaFileName = deltaFilesInfo.isEmpty() ? generateFileName( true ) : deltaFilesInfo.last().absolutePath();
  
  Q_ASSERT( ! mFeatureDeltas->isDirty() );

  mFeatureDeltas.reset( new FeatureDeltas( deltaFileName ) );
}


void LayerObserver::onLayersAdded( const QList<QgsMapLayer *> layers )
{
  for ( QgsMapLayer *layer : layers )
  {
    QgsVectorLayer *vl = dynamic_cast<QgsVectorLayer *>( layer );

    if ( vl && vl->dataProvider() )
    {
      connect( vl, &QgsVectorLayer::beforeCommitChanges, this, &LayerObserver::onBeforeCommitChanges );
      connect( vl, &QgsVectorLayer::committedFeaturesAdded, this, &LayerObserver::onCommittedFeaturesAdded );
      connect( vl, &QgsVectorLayer::committedFeaturesRemoved, this, &LayerObserver::onCommittedFeaturesRemoved );
      connect( vl, &QgsVectorLayer::committedAttributeValuesChanges, this, &LayerObserver::onCommittedAttributeValuesChanges );
      connect( vl, &QgsVectorLayer::committedGeometriesChanges, this, &LayerObserver::onCommittedGeometriesChanges );
      connect( vl, &QgsVectorLayer::editingStopped, this, &LayerObserver::onEditingStopped );
    }
  }
}


void LayerObserver::onBeforeCommitChanges()
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
  QgsVectorLayerEditBuffer *eb = vl->editBuffer();

  if ( ! eb )
    return;

  const QgsFeatureIds deletedFids = eb->deletedFeatureIds();
  // NOTE QgsFeatureIds underlying implementation is QSet, so no need to check if the QgsFeatureId already exists
  QgsFeatureIds changedFids;

  for ( const QgsFeatureId fid : eb->deletedFeatureIds() )
    changedFids.insert( fid );

  for ( const QgsFeatureId fid : eb->changedGeometries().keys() )
    changedFids.insert( fid );

  for ( const QgsFeatureId fid : eb->changedAttributeValues().keys() )
    changedFids.insert( fid );

  QgsChangedFeatures changedFeatures;
  QgsFeatureIterator featuresIt = vl->dataProvider()->getFeatures( QgsFeatureRequest( changedFids ) );
  QgsFeature f;

  // ? is it possible to use the iterator in a less ugly way? something like normal `for ( QgsFeature &f : it ) {}`
  while ( featuresIt.nextFeature( f ) )
    changedFeatures.insert( f.id(), f );

  // NOTE no need to keep track of added features, as they are always present in the layer after commit
  mChangedFeatures.insert( vl->id(), changedFeatures );
  mPatchedFids.insert( vl->id(), QgsFeatureIds() );
}


void LayerObserver::onCommittedFeaturesAdded( const QString &layerId, const QgsFeatureList &addedFeatures )
{
  for ( const QgsFeature &newFeature : addedFeatures )
  {
    mFeatureDeltas->addCreate( layerId, newFeature );
  }
}


void LayerObserver::onCommittedFeaturesRemoved( const QString &layerId, const QgsFeatureIds &deletedFeatureIds )
{
  QgsChangedFeatures changedFeatures = mChangedFeatures.value( layerId );
  
  for ( const QgsFeatureId &fid : deletedFeatureIds )
  {
    Q_ASSERT( changedFeatures.contains( fid ) );

    QgsFeature oldFeature = changedFeatures.take( fid );
    mFeatureDeltas->addDelete( layerId, oldFeature );
  }

  mChangedFeatures.insert( layerId, changedFeatures );
}


void LayerObserver::onCommittedAttributeValuesChanges( const QString &layerId, const QgsChangedAttributesMap &changedAttributesValues )
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
  QgsFeatureIds patchedFids = mPatchedFids.value( layerId );
  QgsChangedFeatures changedFeatures = mChangedFeatures.value( layerId );
  
  for ( const QgsFeatureId &fid : changedAttributesValues.keys() )
  {
    if ( patchedFids.contains( fid ) )
      continue;

    Q_ASSERT( changedFeatures.contains( fid ) );

    patchedFids.insert( fid );

    QgsFeature oldFeature = changedFeatures.take( fid );
    QgsFeature newFeature = vl->getFeature( fid );
    mFeatureDeltas->addPatch( layerId, oldFeature, newFeature );
  }

  mPatchedFids.insert( layerId, patchedFids );
  mChangedFeatures.insert( layerId, changedFeatures );
}


void LayerObserver::onCommittedGeometriesChanges( const QString &layerId, const QgsGeometryMap &changedGeometries )
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
  QgsFeatureIds patchedFids = mPatchedFids.value( layerId );
  QgsChangedFeatures changedFeatures = mChangedFeatures.value( layerId );

  for ( const QgsFeatureId &fid : changedGeometries.keys() )
  {
    if ( patchedFids.contains( fid ) )
      continue;

    Q_ASSERT( changedFeatures.contains( fid ) );
    
    patchedFids.insert(fid);

    QgsFeature oldFeature = changedFeatures.take( fid );
    QgsFeature newFeature = vl->getFeature( fid );

    mFeatureDeltas->addPatch( layerId, oldFeature, newFeature );
  }

  mPatchedFids.insert( layerId, patchedFids );
  mChangedFeatures.insert( layerId, changedFeatures );
}


void LayerObserver::onEditingStopped( )
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );

  mPatchedFids.take( vl->id() );
  mChangedFeatures.take( vl->id() );

  if ( ! mFeatureDeltas->toFile() )
  {
    // TODO somehow indicate the user that writing failed
    QgsLogger::warning( "Failed writing JSON file" );
  }
}
