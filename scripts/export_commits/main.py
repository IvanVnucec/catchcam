import subprocess
import os
import re
import tarfile

export_path = os.path.join(os.path.dirname(__file__), 'exported_commits')

print(f"Exporting commits to {export_path}")

subprocess.check_output(['git', 'checkout', 'master'])

# Get all commits (assuming you want all commits, including initial)
commits = subprocess.check_output(['git', 'log', '--format=%H']).decode('utf-8').splitlines()

for i, commit in enumerate(reversed(commits), start=1):
    # Get the commit message
    commit_message = subprocess.check_output(['git', 'log', '-1', '--format=%s', commit]).decode('utf-8').strip()

    # Convert the commit message to snake case
    snake_case_message = re.sub(r'\W+', '_', commit_message).lower()

    # Create a directory named after the commit message in snake case
    commit_dir = os.path.join(export_path, f"{i}_{snake_case_message}")
    print(f"{i}/{len(commits)}: {commit_message}")
    if not os.path.exists(commit_dir):
        os.makedirs(commit_dir)

    # Create a tarball of the commit
    tarball_path = os.path.join(export_path, f'{commit}.tar')
    subprocess.check_output(['git', 'archive', '--format=tar', commit, '-o', tarball_path])

    # Extract the tarball to the commit directory
    with tarfile.TarFile(tarball_path, 'r') as tar:
        tar.extractall(commit_dir)

    # Remove the tarball
    os.remove(tarball_path)

print("Files extracted successfully.")
