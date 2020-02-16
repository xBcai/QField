#include "qfieldcloudutils.h"
#include <qgsapplication.h>
#include <QString>

const QString QFieldCloudUtils::localCloudDirectory()
{
  QString settingsDirPath = QgsApplication::qgisSettingsDirPath();
  if ( settingsDirPath.right( 1 ) == "/" )
    return settingsDirPath + QStringLiteral( "cloud_projects");
  else
    return settingsDirPath + QStringLiteral( "/cloud_projects");
}
