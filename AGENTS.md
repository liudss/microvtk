# Development Collaboration Mode

## Branching

- Do not commit feature or fix work directly on `master`.
- Do not push directly to remote `master`.
- Enable the committed local pre-push hook once per clone:

```bash
rtk git config core.hooksPath .githooks
```

- Start every code change from the latest remote `master`:

```bash
rtk git fetch origin
rtk git switch master
rtk git merge --ff-only origin/master
rtk git switch -c <type>/<short-description>
```

- Use short-lived branches, for example:
  - `feat/<short-description>`
  - `fix/<short-description>`
  - `chore/<short-description>`
  - `refactor/<short-description>`

## Pull Request Flow

1. Make the change on the short-lived branch.
2. Run the relevant local checks before publishing.
3. Push only the short-lived branch:

```bash
rtk git push -u origin HEAD
```

4. Create a pull request targeting `master`.
5. Wait for CI to pass.
6. Review the PR and decide whether to merge.
7. After merge, update the local `master` from remote:

```bash
rtk git fetch origin
rtk git switch master
rtk git merge --ff-only origin/master
rtk git branch -d <type>/<short-description>
```

## CI and Merge Policy

- CI must pass before merging to `master`.
- Decide whether to merge based on the pull request page status and checks summary.
- Prefer small PRs that are easy to review and revert.
- Keep commits focused on one logical change.
- If a PR is not merged, leave `master` unchanged and delete or revise the branch.
