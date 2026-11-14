# Releasing

This document describes how a maintainer releases a new version of ArgX, the repository settings the release process needs, and how to recover when something goes wrong. How version numbers are chosen is described in [the versioning policy](../references/versioning.md). The changes of each version are listed in [the changelog](../../CHANGELOG.md).

## Overview

A release is made with two pull-request-driven steps. Nothing is tagged or published by hand.

1. A maintainer starts the **Release PR** workflow (`.github/workflows/release-pr.yml`) on `main`, with the version to release. The workflow checks the version, rewrites `include/argx/version.h` and `CHANGELOG.md` on a new branch `release/v<version>`, opens the pull request "Release v\<version\>" to `main`, and starts the **CI** workflow (`.github/workflows/ci.yml`) on that branch.
2. The maintainer reviews the pull request and merges it once the check `CI result` has passed.
3. The merge starts the **Release** workflow (`.github/workflows/release-publish.yml`). It checks the merged commit, creates the annotated tag `v<version>` on it, and creates the GitHub Release `v<version>` with the section of the version in the changelog as its notes.

The same steps make a release (such as `0.2.0`) and a pre-release (such as `0.2.0-rc.1`). A pre-release is marked as a pre-release on GitHub and is never the latest release.

The checks and the rewriting are done by two CMake scripts, which the workflows call and which can also be run locally:

- `tools/release/prepare.cmake` checks a release and rewrites the version header and the changelog (used by **Release PR**).
- `tools/release/publish.cmake` checks a merged commit and makes the notes of the release (used by **Release**).

Neither script uses the network, Git or GitHub.

## One-time repository setup

These settings are made by hand, once, by a maintainer with admin access. The workflows never change any setting of the repository.

1. **Let GitHub Actions open pull requests.** In Settings > Actions > General, under "Workflow permissions", check **Allow GitHub Actions to create and approve pull requests**. Without it, the **Release PR** workflow pushes the release branch but cannot open the pull request. In a repository owned by an organization, the organization must allow this setting too.
2. **Keep the default workflow permissions read-only.** In the same section, "Read repository contents and packages permissions" is enough. The release workflows set `permissions: {}` at the top and ask, job by job, for what they need. CI sets only `contents: read` at the top, which all its jobs use (see [Permissions and secrets](#permissions-and-secrets)).
3. **Protect `main`.** With a branch protection rule or a ruleset on `main`:
   - require a pull request before merging;
   - require the status check **`CI result`**. It is the one check to require: it always ends, with success or failure, also when the builds are skipped for a change to documentation only. Do not require the individual `build (...)` or `quickstart (...)` jobs, since they are skipped for such changes.

   Do not add a rule that stops the creation of branches named `release/v*`: the **Release PR** workflow creates them.
4. **Protecting tags (optional).** If a ruleset protects the tags `v*`, it must still let GitHub Actions create them, since the **Release** workflow pushes the tag. Restrict updates and deletions of the tags, which the workflows never do, but not their creation; or, if your ruleset lets you add GitHub Actions to its bypass list, add it there. A tag that cannot be pushed makes the **Release** workflow fail before the GitHub Release is created.
5. **Enable private vulnerability reporting.** In Settings > Code security, enable **Private vulnerability reporting**. [SECURITY.md](../../SECURITY.md) and the issue template configuration (`.github/ISSUE_TEMPLATE/config.yml`) send vulnerability reports to this form, which does not work until it is enabled.

## Permissions and secrets

The workflows use only the token of the run (`GITHUB_TOKEN`). No other secret, personal access token or GitHub App is needed.

The two release workflows set `permissions: {}` at the top level and give each job only the permissions below. CI sets only `contents: read` at the top level, which all its jobs use, and sets no permission on any job:

| Workflow | Job | Permissions | Why |
| --- | --- | --- | --- |
| CI (`ci.yml`) | all jobs (set at the top level) | `contents: read` | Check out the code. |
| Release PR (`release-pr.yml`) | `release-pr` | `contents: write` | Push the branch `release/v<version>`. |
| | | `pull-requests: write` | Open the release pull request. |
| | | `actions: write` | Start the CI workflow on the release branch. |
| Release (`release-publish.yml`) | `publish` | `contents: write` | Push the tag and create the GitHub Release. |
| | | `pull-requests: read` | Find the merged release pull request when the workflow is started by hand. |

A push or a pull request made with `GITHUB_TOKEN` does not start other workflows. This is why **Release PR** starts the CI workflow on the release branch itself, with `actions: write`, instead of relying on the `pull_request` event.

The **Release** workflow runs on `pull_request`, not `pull_request_target`, and its job runs only for a merged pull request from a branch `release/v*` of this repository. A pull request from a fork never creates a tag or a Release.

## Keeping the changelog

Every pull request with a change visible to users adds an entry under `Unreleased` in [CHANGELOG.md](../../CHANGELOG.md), in the matching category (Added, Changed, Deprecated, Removed, Fixed or Security). The pull request template asks for it. The changelog follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/).

The entries under `Unreleased` become the section of the next version and the notes of its GitHub Release. Before starting a release:

- read the entries under `Unreleased`, and fix their wording, order and categories with an ordinary pull request if needed;
- make sure that every change visible to users since the last release has an entry, and that breaking changes are easy to spot;
- make sure there is at least one entry: the release is refused otherwise (see [Making a pre-release](#making-a-pre-release) for a release with no change since its last pre-release).

Do not write the section of the new version, its date or the links at the end of the file by hand. The **Release PR** workflow does it.

## Making a release

1. Choose the version, as described in [the versioning policy](../references/versioning.md), for example `0.2.0`. Write it without a leading `v`.
2. Open the Actions tab, select the workflow **Release PR**, and select **Run workflow**. Keep the branch `main` and enter the version in the input `version`. The workflow can also be started from the command line:

   ```sh
   gh workflow run release-pr.yml --ref main -f version=0.2.0
   ```

3. The workflow stops, without creating anything, and gives the reason in its log and in the summary of the run, when:
   - it was not started on `main`;
   - the branch `release/v<version>` already exists;
   - the version is not of the form `<major>.<minor>.<patch>[-<pre-release>]` of Semantic Versioning 2.0.0 (build metadata, `+...`, is refused), or one of its numbers is above 255 or has a leading zero;
   - the tag `v<version>` already exists, or the version is not higher than every release tag;
   - the version is lower than the version in `include/argx/version.h`. The version in the header itself is accepted while it has no tag, as for the first release;
   - the changelog is not in the expected form, already lists the version, or has no entry under `Unreleased`.
4. Otherwise the workflow opens the pull request "Release v\<version\>". It changes only two files:
   - `include/argx/version.h`: the version macros are set to the version (unchanged when the header already holds it, as for the first release);
   - `CHANGELOG.md`: the entries under `Unreleased` move to the new section `[<version>] - <date>`, where the date is the day the workflow ran, in UTC; `Unreleased` is left empty; the links at the end are updated.

   The workflow then starts CI on the release branch, and the check `CI result` shows on the pull request when CI ends. The link to the pull request is in the summary of the run.
5. Review the pull request:
   - read the new section of the changelog again; fix it on the release branch if needed;
   - wait for `CI result` to pass on the last commit of the pull request;
   - if the pull request is merged on a later day than the date of the section, the date may be corrected on the release branch.

   Commit only to `include/argx/version.h` and `CHANGELOG.md` on the release branch, and keep the version in both equal to the version of the branch name: the **Release** workflow refuses the merge otherwise. A push by a maintainer to the branch starts CI as for any pull request.
6. Merge the pull request into `main`.
7. The **Release** workflow starts. It checks that the merged commit holds the version in `include/argx/version.h` and as the newest section of `CHANGELOG.md`, then creates:
   - the annotated tag `v<version>` on the merge commit, made by `github-actions[bot]`;
   - the GitHub Release `v<version>`, whose title is `v<version>` and whose notes are the section of the version in the changelog. For a release that follows pre-releases of the same `<major>.<minor>.<patch>`, the notes also refer to those pre-releases.

   No file is attached to the Release. GitHub adds only the archives of the source. The summary of the run lists what was created.
8. Check the tag and the Release on the Releases page of the repository.

## Making a pre-release

A pre-release follows the same steps as a release, with a version that has pre-release identifiers, such as `0.2.0-rc.1`:

```sh
gh workflow run release-pr.yml --ref main -f version=0.2.0-rc.1
```

The differences are:

- The pull request says that it is a pre-release.
- The version header gets `ARGX_VERSION_PRERELEASE` set to the identifiers, such as `"rc.1"`, and the changelog gets the section `[0.2.0-rc.1] - <date>`.
- The GitHub Release is marked as a pre-release and is not made the latest release.

The rules for the version still apply. A pre-release must be higher than every release tag and not lower than the version in the header, so a pre-release of the version already in the header (such as `0.1.0-rc.1` while the header holds `0.1.0`) is refused: use a pre-release of the next version, such as `0.2.0-rc.1`. Further pre-releases and the release itself follow in order of precedence: `0.2.0-rc.1`, `0.2.0-rc.2`, then `0.2.0`. Each one moves only the entries under `Unreleased` at the time, so add entries for the changes made since the previous pre-release before starting the next one.

This holds for the release that follows its pre-releases too: it needs at least one entry under `Unreleased`, since `tools/release/prepare.cmake` refuses an empty `Unreleased`. When nothing changed since the last pre-release, add one entry with an ordinary pull request before starting the release, for example under `### Changed`:

```markdown
- First stable release of 0.2.0, with no change since 0.2.0-rc.2.
```

The changes listed by the pre-releases do not need to be repeated: the notes of the release already refer to the pre-releases of the same `<major>.<minor>.<patch>`.

## When the release pull request shows no CI result

The **Release PR** workflow starts CI on the release branch itself. If the check `CI result` does not show on the pull request, for instance because the last step of **Release PR** failed (its summary says so) or the run of CI was cancelled, start CI again in one of these ways:

- In the Actions tab, select the workflow **CI**, select **Run workflow**, and choose the branch `release/v<version>`. From the command line:

  ```sh
  gh workflow run ci.yml --ref release/v0.2.0
  ```

- Or close the pull request and reopen it. Reopening it as a maintainer starts CI through the `pull_request` event.
- Or, if a run of CI on the branch failed for a temporary reason, re-run it from its page.

The check is reported on the last commit of the branch, so it shows on the pull request whichever way CI was started.

## Recovering from failures

### The Release PR workflow stopped before creating anything

When the workflow fails while checking the branch, the version or the changelog, nothing was created. Read the reason in the log or in the summary of the run, fix the input or the changelog (with an ordinary pull request), and start the workflow again.

### The push of the release branch failed

Nothing was pushed and no pull request was opened. Check that no branch `release/v<version>` exists, and start the workflow again.

### The branch was pushed, but the pull request was not opened

The summary of the run says so. The usual cause is the setting **Allow GitHub Actions to create and approve pull requests** (see [One-time repository setup](#one-time-repository-setup)). Fix the cause, delete the branch `release/v<version>` (on the Branches page, or with `git push origin --delete release/v<version>`), and start the workflow again. The workflow refuses to run while the branch exists.

### The pull request was opened, but CI was not started

The pull request is kept. Start CI by hand, as described in [When the release pull request shows no CI result](#when-the-release-pull-request-shows-no-ci-result).

### Withdrawing a release pull request

To give up a release before it is merged, close the pull request without merging it and delete the branch `release/v<version>`. Nothing reaches `main`: the entries stay under `Unreleased`, and the same version can be started again later.

### The Release workflow failed

The summary of the run gives the reason and lists what was created. Then:

- **The merged commit does not match the version.** The version header or the newest section of the changelog of the merged commit is not the version of the branch name. No tag and no Release were created. The merged commit cannot be published. Make `include/argx/version.h` and `CHANGELOG.md` consistent on `main` with an ordinary pull request. In the changelog, move the entries of the section of the unpublished version back under `Unreleased`, and remove the heading of that section and its link at the end of the file, so that the entries are released with the next version. The next release then proceeds normally: start **Release PR** again, with a version that its checks accept. To release the same version again, first delete the branch `release/v<version>` of the merged pull request, because **Release PR** stops while that branch exists.
- **The tag or the Release could not be created** (for instance, a temporary error of GitHub, or a tag ruleset that stops the push). Fix the cause if there is one, then either re-run the failed run from its page, or start the **Release** workflow by hand: in the Actions tab, select **Release**, select **Run workflow** on `main`, and enter the version, without a leading `v`, in the input `version`:

  ```sh
  gh workflow run release-publish.yml --ref main -f version=0.2.0
  ```

  Started by hand, the workflow finds the merged pull request of the branch `release/v<version>` and uses its merge commit. Both ways create only what is missing: a tag that already points to the merge commit and a Release that already exists are left as they are.
- **The tag exists on another commit.** The workflow never moves a tag, and stops without creating the Release. Find out where the tag comes from. If it was created by mistake and was never part of a release (no GitHub Release uses it), delete it (on the Tags page, or with `git push origin --delete refs/tags/v<version>`) and start the **Release** workflow by hand as above. If it marks something that was published, keep it and release the next version instead.

### Taking back a release published by mistake

1. Delete the GitHub Release `v<version>` on the Releases page.
2. Delete the tag `v<version>` (on the Tags page, or with `git push origin --delete refs/tags/v<version>`).
3. Fix what is wrong on `main` with ordinary pull requests, adding entries under `Unreleased`.
4. Release the fix as the next patch version or the next pre-release.

Never publish a version again under a tag that was published once, even after deleting it: users and package caches may already have it. The changelog keeps the section of the version taken back; describe what happened in the section of the next version.

## Rehearsing the release process

The whole process can be tried in a fork, so that the real repository gets no test tag or Release.

1. Fork the repository. In the fork, enable the workflows in the Actions tab (GitHub disables them in a new fork), and make the settings of [One-time repository setup](#one-time-repository-setup) that the workflows need: allow GitHub Actions to create pull requests, and require `CI result` on `main`.
2. Start **Release PR** on `main` of the fork with a test pre-release, such as `0.2.0-rehearsal.1`. The version must not be lower than the version in the header: a pre-release of the version already in the header (such as `0.1.0-rehearsal.1` while the header holds `0.1.0`) is refused, so use a pre-release of a higher version.

   While the repository has no release yet, rehearse the first release too, before the pre-release: start **Release PR** with the version already in the header, such as `0.1.0`. Its pull request changes only `CHANGELOG.md`, since the header already holds that version, and merging it creates the tag `v0.1.0` and its Release. `Unreleased` is then empty, so add an entry under it with an ordinary pull request before starting the pre-release (see [Making a pre-release](#making-a-pre-release)).
3. Check:
   - that a wrong input stops the workflow without creating anything, for example `0.2.0+build`, `0.2`, or a run started on another branch;
   - the content of the pull request: only `include/argx/version.h` and `CHANGELOG.md` change, and the description says it is a pre-release;
   - that `CI result` shows on the pull request and satisfies the required check;
   - after the merge, that the tag `v0.2.0-rehearsal.1` and a GitHub Release marked as a pre-release are created;
   - that starting **Release** by hand with the same version creates nothing new and reports that the tag and the Release are left as they are;
   - that a pull request that changes only documentation (for example a Markdown file under `docs/`) skips `quickstart` and `build`, and that `CI result` still succeeds and satisfies the required check.
4. When done, delete the tags and the Releases in the fork, and reset or delete the fork, since its `main` now holds the test version.

The scripts can also be tried locally, without GitHub. Copy `include/argx/version.h` and `CHANGELOG.md` into a scratch directory with the same layout (or use a copy of the working tree), list the tags, and run `prepare.cmake` on the copy. With `ARGX_RELEASE_CHECK_ONLY=ON` it runs every check and writes nothing:

```sh
git tag --list 'v*' > /tmp/tags.txt
cmake -D ARGX_RELEASE_VERSION=0.2.0-rc.1 \
      -D ARGX_RELEASE_DATE=2026-01-15 \
      -D ARGX_RELEASE_REPO_URL=https://github.com/<owner>/<repo> \
      -D ARGX_SOURCE_DIR=/tmp/argx-copy \
      -D ARGX_RELEASE_TAGS_FILE=/tmp/tags.txt \
      -D ARGX_RELEASE_CHECK_ONLY=ON \
      -P tools/release/prepare.cmake
```

Without `ARGX_RELEASE_CHECK_ONLY`, it rewrites the two files of the copy. `publish.cmake` then checks the rewritten copy and writes the notes:

```sh
cmake -D ARGX_RELEASE_VERSION=0.2.0-rc.1 \
      -D ARGX_RELEASE_REPO_URL=https://github.com/<owner>/<repo> \
      -D ARGX_SOURCE_DIR=/tmp/argx-copy \
      -D ARGX_RELEASE_NOTES=/tmp/notes.md \
      -P tools/release/publish.cmake
```

Both scripts write only the files they are given, and use no network, Git or GitHub. Their automated tests run with the other tests, and can be run alone with `ctest -L release` in a build directory.

### Before the first release

Before the first release, check that the FetchContent example in README.md pins the tag you are about to create. If it does not, fix README.md with a separate pull request, merged before the release pull request: a release pull request changes only `include/argx/version.h` and `CHANGELOG.md`.
