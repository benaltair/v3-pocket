#!/usr/bin/env bash
set -euo pipefail
[[ "$TAG" =~ ^repeater-v[0-9]+\.[0-9]+\.[0-9]+$ && "$COMMIT" =~ ^[a-f0-9]{40}$ ]]
branch="updates/$TAG"
existing=$(gh api -X GET "repos/$GITHUB_REPOSITORY/pulls" -f head="${GITHUB_REPOSITORY%%/*}:$branch" -f base="$BASE" -f state=all --jq 'length')
if [[ "$existing" != 0 ]]; then
  echo "An update PR already exists for $TAG."
  exit 0
fi
jq -n --arg tag "$TAG" --arg commit "$COMMIT" '{tag:$tag, commit:$commit}' > upstream.json
if git diff --quiet -- upstream.json; then exit 0; fi
if git ls-remote --exit-code --heads origin "refs/heads/$branch" >/dev/null; then
  git fetch origin "refs/heads/$branch"
  git show FETCH_HEAD:upstream.json | cmp - upstream.json
else
  git switch -c "$branch"
  git add upstream.json
  git -c user.name='github-actions[bot]' -c user.email='41898282+github-actions[bot]@users.noreply.github.com' commit -m "Track MeshCore $TAG"
  git push origin "HEAD:refs/heads/$branch"
fi
cat > "$RUNNER_TEMP/update-body.md" <<EOF
Updates the pinned MeshCore repeater release to [$TAG](https://github.com/meshcore-dev/MeshCore/releases/tag/$TAG) at commit \`$COMMIT\`.

The [build and test run]($GITHUB_SERVER_URL/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID) finished with **$RESULT**. A successful build includes the application and its checksum. Review the changes and check the OLED, packet graph, PRG button and saved settings on a Heltec WiFi LoRa 32 V3 before merging.
EOF
set --
if [[ -n "${REVIEWER:-}" ]]; then set -- --reviewer "$REVIEWER"; fi
gh pr create "$@" --draft --base "$BASE" --head "$branch" --title "Update V3 Pocket to $TAG" --body-file "$RUNNER_TEMP/update-body.md"
