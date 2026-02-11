#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Git credential helper: reads ~/.github/credentials.txt (no TTY needed).
File format: username=... and personal_access_token=...

Usage: git config credential.helper "path/to/scripts/git/git_credential_github.py"
"""
import os
import sys


def main():
    if len(sys.argv) < 2 or sys.argv[1] != "get":
        sys.exit(0)
    creds = os.path.join(os.path.expanduser("~"), ".github", "credentials.txt")
    if not os.path.isfile(creds) or not os.access(creds, os.R_OK):
        sys.exit(1)
    with open(creds, "r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("username="):
                print("username=" + line.split("=", 1)[1])
            elif line.startswith("personal_access_token="):
                print("password=" + line.split("=", 1)[1])
    sys.exit(0)


if __name__ == "__main__":
    main()
