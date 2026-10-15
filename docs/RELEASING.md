# Builds, branch protection and releases

## Automatic builds

`.github/workflows/build.yml` runs on every pull request and every merge to `main`:

| Job | Runner | Result |
|---|---|---|
| Linux | Ubuntu 22.04, GCC 13 | `Unicorn-Overlord-x86_64.AppImage` (runs on glibc 2.35 and newer: Ubuntu 22.04+, Debian 12+, SteamOS 3.5+) |
| Windows | MSYS2 CLANG64 | `Unicorn Overlord.exe` (one file; vwport and its libraries inside) |
| Publish latest | main only | Replaces the `latest` pre-release with both files |

Every run keeps both files as workflow artifacts (Actions tab → the run → Artifacts), so a pull
request can be tested before it is merged. Both packages store their version (`git describe`) in
`VERSION`; `run.py` writes it at the top of every run log.

The Vulkan headers, glslang, VMA and robin-map versions are pinned in the workflow's `env:`
block. The static libraries (FFmpeg, SDL3, ...) are pinned in `packaging/deps.sh`. Both are
cached; changing a pin rebuilds it.

Locally: `python3 packaging/package.py unicorn` then `bash packaging/linux/appimage.sh "Unicorn Overlord"`
(Linux), or `bash packaging/windows/build.sh unicorn` in MSYS2 CLANG64.

## Branch protection (repository owner, once)

`.github/main-protection.json` is a repository ruleset for the default branch:

- no direct pushes: every change goes through a pull request, admins included
- one approving review; a new push dismisses earlier approvals
- the `Linux` and `Windows` build jobs must pass
- no force-push, no deletion

Apply it with the GitHub CLI (needs admin rights on the repository):

```bash
gh api -X POST repos/hachi23/unicorn_overlord_pc/rulesets --input .github/main-protection.json
```

Or in the web interface: Settings → Rules → Rulesets → New ruleset → Import a ruleset, and pick
the file.

The status checks only appear after the workflow has run once, so merge the pull request that
adds the workflow first, then apply the ruleset.

**Single maintainer:** GitHub does not let anyone approve their own pull request. With one
required approval and no bypass, a maintainer working alone can never merge. To let repository
admins merge their own pull requests (still never push directly), replace `"bypass_actors": []`
with:

```json
"bypass_actors": [{ "actor_id": 5, "actor_type": "RepositoryRole", "bypass_mode": "pull_request" }]
```

(`5` is GitHub's id for the admin role.)

## Making a release

1. Merge to `main`. The `latest` pre-release updates by itself.
2. Play the `latest` build: title, new game, the first battle, save and load, on Linux and
   Windows.
3. Tag the tested commit and publish a versioned release from it:
   ```bash
   git tag -a v0.1.0 -m "v0.1.0" <commit> && git push origin v0.1.0
   gh release create v0.1.0 --prerelease --title "v0.1.0" --notes-file notes.md \
       Unicorn-Overlord-x86_64.AppImage "Unicorn Overlord.exe"
   ```
   Use the files from that commit's workflow run. Mark releases as pre-releases until the game
   is playable from start to end.
