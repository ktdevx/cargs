# Releasing

How a maintainer releases ArgX. How to choose the version is in [Compatibility](../references/version.md#compatibility).

## Overview

Nothing is tagged or published by hand.

1. Start the **Release PR** workflow on `main` with the version. It rewrites `include/argx/version.h` and `CHANGELOG.md` on the branch `release/v<version>`, opens a pull request, and starts CI on it.
2. Review the pull request, and merge it once `CI result` has passed.
3. The merge starts the **Release** workflow, which creates the tag `v<version>` and the GitHub Release, with the changelog section as its notes.

The work is done by `tools/release/prepare.cmake` and `tools/release/publish.cmake`, which use no network, Git or GitHub and can be run locally ([Rehearsing](#rehearsing-the-release-process)).

## One-time repository setup

An admin makes these settings by hand; the workflows never change settings.

1. **Settings > Actions > General:** check **Allow GitHub Actions to create and approve pull requests**, and keep the default workflow permissions read-only.
2. **Protect `main`:** require a pull request and the status check **`CI result`**. Do not require the individual `build (...)` or `quickstart (...)` jobs; they are skipped for documentation-only changes. Do not block the creation of `release/v*` branches.
3. **Tag rulesets (optional):** if `v*` tags are protected, still let GitHub Actions create them.
4. **Settings > Code security:** enable **Private vulnerability reporting**, which [SECURITY.md](../../SECURITY.md) relies on.

## Permissions and secrets

The workflows use only `GITHUB_TOKEN`. The release workflows set `permissions: {}` at the top and grant per job:

| Workflow | Job | Permissions | Why |
| --- | --- | --- | --- |
| CI (`ci.yml`) | all | `contents: read` | Check out the code |
| Release PR (`release-pr.yml`) | `release-pr` | `contents: write` | Push the release branch |
| | | `pull-requests: write` | Open the pull request |
| | | `actions: write` | Start CI, since pushes made with `GITHUB_TOKEN` do not trigger workflows |
| Release (`release-publish.yml`) | `publish` | `contents: write` | Push the tag and create the Release |
| | | `pull-requests: read` | Find the merged pull request when started by hand |

**Release** runs on `pull_request` (not `pull_request_target`) and only for a merged `release/v*` branch of this repository, so forks cannot publish.

## Keeping the changelog

[CHANGELOG.md](../../CHANGELOG.md) follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/). Every pull request with a user-visible change adds an entry under `Unreleased` in the matching category.

Before a release, review the `Unreleased` entries with an ordinary pull request if needed. There must be at least one entry. Do not write the version section, its date or the links at the end by hand.

## Making a release

1. Start **Release PR** on `main` with the version, without a leading `v`, from the Actions tab or:

   ```sh
   gh workflow run release-pr.yml --ref main -f version=0.2.0
   ```

   If the branch, the version or the changelog is wrong, it stops without creating anything and gives the reason in the run summary.
2. Review the pull request. You may fix the changelog section, or its date if you merge on a later day, on the release branch. Change no other files, and keep the version equal to the branch name.
3. Merge it once `CI result` has passed.
4. Check the tag and the Release on the Releases page.

## Making a pre-release

Use a version with pre-release identifiers. The Release is marked as a pre-release and never becomes the latest.

```sh
gh workflow run release-pr.yml --ref main -f version=0.2.0-rc.1
```

- The version must be higher than every release tag, so pre-release the next version (`0.2.0-rc.1`), not the one already released.
- Each pre-release, and the final release after it, moves only the current `Unreleased` entries and needs at least one. If nothing changed since the last pre-release, add an entry such as `- First stable release of 0.2.0, with no change since 0.2.0-rc.2.` The release notes already refer to the pre-releases.

## When the release pull request shows no CI result

Start CI on the release branch again: run **CI** on `release/v<version>` from the Actions tab (`gh workflow run ci.yml --ref release/v0.2.0`), close and reopen the pull request, or re-run a failed run.

## Recovering from failures

- **Release PR failed before opening the pull request.** Fix the cause given in the run summary, delete `release/v<version>` if it was pushed, and start again. A missing pull request is usually the setting in [One-time repository setup](#one-time-repository-setup).
- **Withdrawing a release.** Close the pull request without merging and delete the branch. The entries stay under `Unreleased`.
- **Release refused the merged commit** (the version header or the newest changelog section does not match the branch). Nothing was published. With an ordinary pull request, move the section's entries back under `Unreleased` and remove its heading and link. Delete the old release branch before releasing the same version again.
- **The tag or the Release could not be created.** Fix the cause, then re-run the run, or start **Release** by hand. It creates only what is missing.

  ```sh
  gh workflow run release-publish.yml --ref main -f version=0.2.0
  ```

- **The tag exists on another commit.** The workflow never moves tags. If the tag was never published, delete it and start **Release** by hand; otherwise release the next version.

### Taking back a release published by mistake

Delete the GitHub Release and the tag (`git push origin --delete refs/tags/v<version>`), fix `main`, and release the next patch or pre-release. Never reuse a published version. Keep its changelog section, and explain what happened in the next one.

## Rehearsing the release process

To try the workflows, fork the repository, enable Actions and the [setup](#one-time-repository-setup) in the fork, and release a test pre-release such as `0.2.0-rehearsal.1`. Delete the fork afterwards.

To try the scripts locally, run them on a copy of the working tree. `ARGX_RELEASE_CHECK_ONLY=ON` checks without writing; without it, `prepare.cmake` rewrites the copy, and `publish.cmake` then checks it and writes the notes:

```sh
git tag --list 'v*' > /tmp/tags.txt
cmake -D ARGX_RELEASE_VERSION=0.2.0-rc.1 \
      -D ARGX_RELEASE_DATE=2026-01-15 \
      -D ARGX_RELEASE_REPO_URL=https://github.com/<owner>/<repo> \
      -D ARGX_SOURCE_DIR=/tmp/argx-copy \
      -D ARGX_RELEASE_TAGS_FILE=/tmp/tags.txt \
      -D ARGX_RELEASE_CHECK_ONLY=ON \
      -P tools/release/prepare.cmake

cmake -D ARGX_RELEASE_VERSION=0.2.0-rc.1 \
      -D ARGX_RELEASE_REPO_URL=https://github.com/<owner>/<repo> \
      -D ARGX_SOURCE_DIR=/tmp/argx-copy \
      -D ARGX_RELEASE_NOTES=/tmp/notes.md \
      -P tools/release/publish.cmake
```

Their automated tests run with `ctest -L release`.
