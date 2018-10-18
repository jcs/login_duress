# login_duress

`login_duress` is a
[BSD authentication](http://man.openbsd.org/auth_subr.3)
module providing duress functionality upon authentication.
The concept is modeled after the
[`pam_duress`](https://github.com/rafket/pam_duress)
module for PAM.

It essentially adds a per-user backdoor password which, when entered instead of
the user's normal password, will also execute a particular command as that
user.

## Installation

Compile and install the module with `make install`.

## Database Setup

Create a duress database at `/etc/duress` in the format of:

    user:encrypted password:duress command

The encrypted password hash can be generated with `encrypt -b10 -p`.

The command in the duress file will be executed as the user logging in
(via `sh -c`), so any functionality requiring elevated privileges should use
`doas` or `sudo` configured to allow the command without a password.

Once the duress database has been created, make sure its ownership is correct:

    # chown root:wheel /etc/duress
    # chmod 600 /etc/duress

`duress` can now be activated as the default authentication type in
`/etc/login.conf` by changing `auth-defaults` (or by assigning a new `auth` key
for the specific user class):

    auth-defaults:auth=-duress:

This will allow `duress` logins when authenticating via `login(1)`, `doas(8)`,
`sudo(8)`, `xenodm(1)`, `ssh(1)` (if you allow password authentication, which
you shouldn't), and everything else that uses BSD authentication on the system.

## Example

Example `/etc/duress` database:

    jcs:$2b$10$PxqDTkwaNak6mUUi1aNWweUZYGIVwE90kYnPkt8HE1HrFWdt84Snm:/usr/bin/doas /sbin/halt -pnq

Example `doas.conf`:

    permit nopass jcs cmd /sbin/halt

*Note: `doas` uses the last matching entry, so if you also have the default
"`permit persist keepenv :wheel`", the `nopass` entry must be added after that
`permit` line.*

Now when the user `jcs` logs in with the password that matches the hash in
`/etc/duress` instead of the normal password database, the command
`/usr/sbin/doas /sbin/halt -pnq` will be run, immediately powering off the
machine.

This is not a particularly subtle example, so perhaps a better one might be to
run a script that just deletes (via `rm -P`) the user's sensitive files such
as SSH keys, e-mail archives, etc.

Perhaps an even more subtle example is to run just `/usr/bin/true` upon duress
login.
This does nothing but allow one to login with an alternative password in the
scenario where some agency is requiring the user to reveal their password so
they can login and poke around, but that user's normal system password is also
used on other, more sensitive systems and cannot be revealed.
