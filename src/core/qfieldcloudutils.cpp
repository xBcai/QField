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

#include "qfieldcloudutils.h"
#include <qgsapplication.h>
#include <QDir>
#include <QString>
#include <QDebug>

const QString QFieldCloudUtils::localCloudDirectory()
{
  return QDir::cleanPath( QgsApplication::qgisSettingsDirPath() ) + QStringLiteral( "/cloud_projects" );
}

const QString QFieldCloudUtils::localProjectFilePath( const QString &projectId )
{
  QString project = QStringLiteral( "%1/%2" ).arg( QFieldCloudUtils::localCloudDirectory(), projectId );
  QDir projectDir( project );
  QStringList projectFiles = projectDir.entryList( QStringList() << QStringLiteral( "*.qgz" ) << QStringLiteral( "*.qgs" ) );
  if ( projectFiles.count() > 0 )
  {
    return QStringLiteral( "%1/%2" ).arg( project, projectFiles.at( 0 ) );
  }
  return QString();
}
