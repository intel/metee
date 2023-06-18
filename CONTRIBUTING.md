# Contributing

### License

MeTee library is licensed under the terms in [LICENSE](COPYING).
By contributing to the project, you agree to the license and copyright
terms therein and release your contribution under these terms.

### Git Commit Messages

### A properly formed git commit subject line should always be able to complete the following sentence
If applied, this commit will *\<your subject line here\>*

### Rules for a great git commit message style

* Separate subject from body with a blank line.
* Do not end the subject line with a period, it will disturb the patch creation.
* Capitalize each paragraph.
* Use the imperative mood.
* Wrap lines at 72 character.s
* Use the body to explain what and why you have done something,
  if change is complex also explain how.

### Information in commit messages

* Describe why a change is being made.
* How does it address the issue?
* What effects does the patch have?
* Do not assume the reviewer understands what the original problem was.
* Do not assume the code is self-evident/self-documenting.
* Read the commit message to see if it hints at improved code structure.
* The first commit line is the most important.
* Describe any limitations of the current code.
* Do not include patch set-specific comments.

Details for each point and good commit message examples can be found on https://wiki.openstack.org/wiki/GitCommitMessages#Information_in_commit_messages

### References in commit messages

* Structure:
  ```
  MeTee: <subsection>: <title>
  <blank line>
  <body>
  Signed-off-by: Your Name <your.email@yyy.zzz>
  ```
* Possible subsection
  - github: changes in github management: all under .github/
  - Linux: Linux os specific changes.
  - Windows: Windows os specific changes.
  - CMake: changes in the cmake build configuration.
  - tests: changes in tests.
  - samples: changes in samples.
  - conan: changes in conan configuration.
  - doc: changes in documentation.

### Sign your work

Please use the sign-off line at the end of the patch. Your signature
certifies that you wrote the patch or otherwise have the right to pass
it on as an open-source patch. The rules are pretty simple: if you can
certify the below
(from [developercertificate.org](http://developercertificate.org/)):


```
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
660 York Street, Suite 102,
San Francisco, CA 94110 USA

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.

Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

Then you just add a line to every git commit message:

    Signed-off-by: Joe Smith <joe.smith@email.com>

Use your real name (sorry, no pseudonyms or anonymous contributions.)

If you set your `user.name` and `user.email` git configs, you can sign your
commit automatically with `git commit -s`.
