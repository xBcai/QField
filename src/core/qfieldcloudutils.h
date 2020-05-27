/***************************************************************************
    qfieldcloudutils.cpp
    ---------------------
    begin                : February 2020
    copyright            : (C) 2020 by Mathieu Pellerin
    email                : nirvn dot asia at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QFIELDCLOUDUTILS_H
#define QFIELDCLOUDUTILS_H

#include <qgsmaplayer.h>
#include <qfieldcloudprojectsmodel.h>

class QString;
class QFieldCloudProjectsModel;

class QFieldCloudUtils
{
  public:
  
  static const QString localCloudDirectory();
  static const QString localProjectFilePath( const QString &projectId );

  /**
   * Get the \layer action at QFieldCloud.
   * 
   * @param layer to be checked
   * @return const QFieldCloudProjectsModel::LayerAction action of the layer
   */
  static const QFieldCloudProjectsModel::LayerAction layerAction( const QgsMapLayer *layer );
};

#endif // QFIELDCLOUDUTILS_H
