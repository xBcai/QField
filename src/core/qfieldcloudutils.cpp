#include "qfieldcloudutils.h"
#include <qgsapplication.h>
#include <QDir>
#include <QString>
#include <QDebug>

const QString QFieldCloudUtils::localCloudDirectory()
{
  return QDir::cleanPath( QgsApplication::qgisSettingsDirPath() ) + QStringLiteral( "/cloud_projects");
}

const QString QFieldCloudUtils::localProjectFilePath( const QString &owner, const QString &projectName )
{
  QString project = QStringLiteral( "%1/%2/%3" ).arg( QFieldCloudUtils::localCloudDirectory(), owner, projectName );
  QDir projectDir( project );
  QStringList projectFiles = projectDir.entryList( QStringList() << QStringLiteral( "*.qgz" ) << QStringLiteral( "*.qgs" ) );
  if ( projectFiles.count() > 0 )
  {
    return QStringLiteral(  "%1/%2" ).arg( project, projectFiles.at( 0 ) );
  }
  return QString();
}
