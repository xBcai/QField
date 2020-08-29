#!/usr/bin/env bash

source ${CI_BUILD_DIR}/../version_number.sh

lupdate -recursive ${CI_BUILD_DIR} -ts $(ls ${CI_BUILD_DIR}/i18n/*.ts)

# release only if the branch is master
if [[ ${CI_BRANCH} = master ]]; then
  tx push --source
fi

# release only if the branch is revive_translations
if [[ ${CI_BRANCH} = revive_translations ]]; then
  tx push --source
fi
