#!/usr/bin/env python3
"""Publish one validated build through GitHub CLI, keeping partial uploads draft."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

REPO = 'zebbel/MeshCore'
BRANCH = 'meshcoreStation'
ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    result = subprocess.run(args, cwd=ROOT, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or f'{args[0]} command failed')
    return result.stdout.strip()


def check(commit, version):
    if not re.fullmatch(r'[0-9a-f]{40}', commit) or not re.fullmatch(r'v[0-9]+\.[0-9]+\.[0-9]+', version):
        raise ValueError('Invalid commit or custom version')
    if run('git', 'rev-parse', 'HEAD') != commit or run('git', 'status', '--porcelain', '--untracked-files=normal'):
        raise RuntimeError('Commit all source changes before releasing; checkout must be clean.')
    run('gh', 'auth', 'status', '--hostname', 'github.com')
    remote = run('gh', 'api', f'repos/{REPO}/commits/{BRANCH}', '--jq', '.sha')
    if remote != commit:
        raise RuntimeError(f'Push this exact commit to {REPO}:{BRANCH} before releasing.')
    tag = f'meshcoreStation-{version}'
    # Successful listings distinguish absence from authentication/network errors.
    releases = run('gh', 'api', f'repos/{REPO}/releases', '--paginate', '--jq', '.[].tag_name').splitlines()
    refs = run('gh', 'api', f'repos/{REPO}/git/matching-refs/tags/{tag}', '--jq', '.[].ref').splitlines()
    if tag in releases or f'refs/tags/{tag}' in refs:
        raise RuntimeError(f'{tag} already exists. Bump meshcoreStation.version; existing releases are never overwritten.')
    return tag


def publish(commit, version, assets):
    tag = check(commit, version)
    paths = [Path(p).resolve() for p in assets]
    if len(paths) != 7 or len({p.name for p in paths}) != 7:
        raise ValueError('Expected the seven files from one completed dual build')
    for path in paths:
        if path.parent != ROOT / 'release' or not path.is_file() or not path.stat().st_size:
            raise ValueError('Missing or unexpected release asset')
        if f'-meshcoreStation-{version}.' not in path.name:
            raise ValueError('Release asset version mismatch')
    notes = (f'meshcoreStation custom firmware {version}\n\nSource commit: {commit}\n\n'
             'Heltec V4 OLED USB companion: application .bin + .json for updates; '
             'the separate merged .bin is for full flashing.\n'
             'Seeed T1000-E BLE companion: private-channel position button; UF2 or Nordic DFU ZIP.\n'
             'Position sending is disabled until configured. See docs/t1000-position-button.md.\n')
    with tempfile.TemporaryDirectory(prefix='station-release-') as folder:
        notes_file = Path(folder) / 'notes.md'
        notes_file.write_text(notes)
        run('gh', 'release', 'create', tag, *map(str, paths), '--repo', REPO,
            '--target', commit, '--draft', '--title', f'meshcoreStation {version}',
            '--notes-file', str(notes_file))
    # Publish only once all expected assets are present with their complete sizes.
    uploaded = json.loads(run('gh', 'release', 'view', tag, '--repo', REPO, '--json', 'assets'))['assets']
    expected = {p.name: p.stat().st_size for p in paths}
    actual = {p['name']: p['size'] for p in uploaded}
    if actual != expected:
        raise RuntimeError(f'Upload verification failed; {tag} remains a draft.')
    run('gh', 'release', 'edit', tag, '--repo', REPO, '--draft=false', '--latest')
    print(f'Published https://github.com/{REPO}/releases/tag/{tag}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('commit')
    parser.add_argument('version')
    parser.add_argument('assets', nargs='*')
    args = parser.parse_args()
    try:
        if args.check:
            check(args.commit, args.version)
        else:
            publish(args.commit, args.version, args.assets)
    except (RuntimeError, ValueError, OSError) as exc:
        raise SystemExit(str(exc))
