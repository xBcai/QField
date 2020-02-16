#ifndef QFIELDCLOUDUTILS_H
#define QFIELDCLOUDUTILS_H

class QString;

class QFieldCloudUtils
{
  public:
  
  static const QString localCloudDirectory();
  static const QString localProjectFilePath( const QString &owner, const QString &projectName );
};

#endif // QFIELDCLOUDUTILS_H
