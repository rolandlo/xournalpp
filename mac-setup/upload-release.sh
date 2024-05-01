#!/usr/bin/env bash
# See https://cirrus-ci.org/examples/#release-assets

if [[ "$CIRRUS_RELEASE" == "" ]]; then
  echo "Not a release. No need to deploy!"
  exit 0
fi

if [[ "$GITHUB_TOKEN" == "" ]]; then
  echo "Please provide GitHub access token via GITHUB_TOKEN environment variable!"
  exit 1
fi

file_content_type="application/zip"
asset_path=xournalpp.zip # relative path of asset to upload
curl -o $asset_path https://api.cirrus-ci.com/v1/artifact/build/$CIRRUS_BUILD_ID/build_xournalpp/xournalpp.zip
ver=$(echo $CIRRUS_TAG | sed s/v//)    # --> x.y.z

name="xournalpp-$ver-macos_arm64.zip"  # --> xournalpp-x.y.z-macos_arm64.zip

echo "Uploading $asset_path as $name ..."
url_to_upload="https://uploads.github.com/repos/$CIRRUS_REPO_FULL_NAME/releases/$CIRRUS_RELEASE/assets?name=$name"
curl -X POST \
    --data-binary @$asset_path \
    --header "Authorization: token $GITHUB_TOKEN" \
    --header "Content-Type: $file_content_type" \
    $url_to_upload
