## What and why

<!-- A few sentences. Link the issue: "Fixes #123". -->

## How it was tested

- [ ] `bash build.sh`
- [ ] `python -m unittest discover -s tests`
- [ ] `bash build.sh --test`
- [ ] `--check-imports` shows no new missing imports
- [ ] Played in game. Scene(s) and how far:

OS / GPU / driver:

## Before / after

<!-- FPS and stage A/B µs per draw for performance changes (same scene, same machine).
     Screenshots for visual changes. Delete if not relevant. -->

## Risk

<!-- What could break? Do saves, vw.ini keys or the out/ layout change? -->

## Checklist

- [ ] One topic in this PR
- [ ] No game files, saves, keys or dumps (`git status` checked)
- [ ] New logic has a test in `tests/`
- [ ] Docs updated (README, docs/MODS.md, docs/unicorn/HANDOFF.md) if behaviour or options changed
- [ ] Commit subjects follow `Area: what changed`
