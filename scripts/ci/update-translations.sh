#!/usr/bin/env bash

source ${CI_BUILD_DIR}/scripts/version_number.sh

lupdate -recursive ${CI_BUILD_DIR} -ts ${CI_BUILD_DIR}/i18n/qfield_en.ts
lupdate -recursive ${CI_BUILD_DIR} -ts ${CI_BUILD_DIR}/i18n/qfield_bg.ts

echo ==================1
grep Changelog ${CI_BUILD_DIR}/i18n/qfield_en.ts
echo ==================1


echo ==================2
grep Changelog ${CI_BUILD_DIR}/i18n/qfield_bg.ts
echo ==================2

# release only if the branch is master
if [[ ${CI_BRANCH} = master ]]; then
  tx push --source
fi

# release only if the branch is revive_translations
if [[ ${CI_BRANCH} = revive_translations ]]; then
  tx push --source
fi
