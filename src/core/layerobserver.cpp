#include "layerobserver.h"


#include <qgsfeature.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsvectorlayereditbuffer.h>


// ? should I keep this a general purpose observer or make it LayerDeltaObserver?
LayerObserver::LayerObserver( const QgsProject *project )
  : mProject( project )
{
  mFeatureDeltas = new FeatureDeltas( QStringLiteral( "/home/suricactus/qgis/filename.json" ) );
  connect( project, &QgsProject::layersAdded, this, &LayerObserver::onLayersAdded );
}


void LayerObserver::onLayersAdded( const QList<QgsMapLayer *> layers )
{
  for ( QgsMapLayer *layer : layers )
  {
    QgsVectorLayer *vl = dynamic_cast<QgsVectorLayer *>( layer );

    if ( vl && vl->dataProvider() )
    {
      // ? what happens when a layer is removed or the project is closed? Are these event listener automatically discarded?
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
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );

    for ( const QgsFeature &newFeature : addedFeatures )
    {
        mFeatureDeltas->addCreate( vl, newFeature );
    }
}


void LayerObserver::onCommittedFeaturesRemoved( const QString &layerId, const QgsFeatureIds &deletedFeatureIds )
{
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
    QgsChangedFeatures changedFeatures = mChangedFeatures.value( vl->id() );
    
    for ( const QgsFeatureId &fid : deletedFeatureIds )
    {
        Q_ASSERT( changedFeatures.contains( fid ) );

        QgsFeature oldFeature = changedFeatures.take( fid );
        mFeatureDeltas->addDelete( vl, oldFeature );
    }

    mChangedFeatures.insert( vl->id(), changedFeatures );
}


void LayerObserver::onCommittedAttributeValuesChanges( const QString &layerId, const QgsChangedAttributesMap &changedAttributesValues )
{
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
    QgsFeatureIds patchedFids = mPatchedFids.value( vl->id() );
    QgsChangedFeatures changedFeatures = mChangedFeatures.value( vl->id() );
    
    for ( const QgsFeatureId &fid : changedAttributesValues.keys() )
    {
        Q_ASSERT( changedFeatures.contains( fid ) );

        if ( patchedFids.contains( fid ) )
          continue;

        patchedFids.insert( fid );

        QgsFeature oldFeature = changedFeatures.take( fid );
        QgsFeature newFeature = vl->getFeature( fid );
        mFeatureDeltas->addPatch( vl, oldFeature, newFeature );
    }

    mPatchedFids.insert( vl->id(), patchedFids );
    mChangedFeatures.insert( vl->id(), changedFeatures );
}


void LayerObserver::onCommittedGeometriesChanges( const QString &layerId, const QgsGeometryMap &changedGeometries )
{
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( sender() );
    QgsFeatureIds patchedFids = mPatchedFids.value( vl->id() );
    QgsChangedFeatures changedFeatures = mChangedFeatures.value( vl->id() );

    for ( const QgsFeatureId &fid : changedGeometries.keys() )
    {
        Q_ASSERT( changedFeatures.contains( fid ) );

        if ( patchedFids.contains( fid ) )
          continue;
        
        patchedFids.insert(fid);

        QgsFeature oldFeature = changedFeatures.take( fid );
        QgsFeature newFeature = vl->getFeature( fid );

        mFeatureDeltas->addPatch( vl, oldFeature, newFeature );
    }

    mPatchedFids.insert( vl->id(), patchedFids );
    mChangedFeatures.insert( vl->id(), changedFeatures );
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
