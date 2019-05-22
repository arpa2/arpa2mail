# ARPA2 ACL milter

The a2aclmilter daemon tests whether a sender may communicate with a receiver
given an [ARPA2] ACL policy.

Status: **beta**

## Requirements

Build requirements:
* A C89 compiler
* CMake >= 3.3
* liba2acl
* libmilter >= 8.16

Run-time requirements:
* Any POSIX-compliant system


## Usage example

In order to let a mailserver like Postfix query the milter for every incoming
mail about whether or not communication is allowed, start the milter with the
desired policy file, and configure Postfix to use the milter. In this example
the policy file `/etc/a2aclpolicy` is used. The milter is instructed to drop
privileges to the unprivileged user id 2498 and chroot to /etc/opt. Let it
listen on localhost port 7000 for incoming connections:

```sh
a2aclmilter /etc/a2aclpolicy 2498 /etc/opt inet:7000@127.0.0.1
```

In order to let Postfix communicate with the milter add the following line to
`/etc/postfix/main.cf`:

```
smtpd_milters = inet:127.0.0.1:7000
```

For a more complete and interactive example see the [Docker demo].


## Installation

First install liba2acl from the libarpa2service repository:
https://github.com/arpa2/libarpa2service

Then install a2aclmilter:
```sh
$ git clone https://github.com/arpa2/arpa2mail.git
$ mkdir arpa2mail/a2aclmilter/build
$ cd arpa2mail/a2aclmilter/build
$ cmake ..
$ make
$ sudo make install
```


## Documentation

For further documentation please refer to [ARPA2 Identifier and ACL introduction]
and the manpages:
* [a2acl(1)]
* [a2acl.conf(5)]
* [a2aclmilter(8)]


## License

ISC

Copyright (c) 2019 Tim Kuijsten

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright notice
and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

[ARPA2]: http://arpa2.net
[ARPA2 Identifier and ACL introduction]: /doc/design/a2idacl-intro.md
[a2acl(1)]: https://netsend.nl/arpa2/a2acl.1.html
[a2acl.conf(5)]: https://netsend.nl/arpa2/a2acl.conf.5.html
[a2aclmilter(8)]: https://netsend.nl/arpa2/a2aclmilter.8.html
[Docker demo]: https://github.com/arpa2/docker-demo/tree/master/demo-a2aclmilter
