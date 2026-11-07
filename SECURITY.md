# Security

Namaz calculates prayer times locally and does not require an account.  The
only network request is the optional update check against the GitHub Releases
API (`api.github.com`); it sends no user data.

## Reporting a vulnerability

Please do not open a public issue for security-sensitive reports.  Contact the
maintainer through the GitHub profile:

https://github.com/ikhlasulov-gb

Include the affected version, a description of the issue and, if possible, a
minimal reproduction.  You will receive an acknowledgement as soon as
possible.

## Flatpak signing

Release Flatpak repositories are signed with a dedicated GPG key.  The public
key is embedded in `namaz.flatpakrepo`.  Do not install builds from a remote
whose key you have not verified.
