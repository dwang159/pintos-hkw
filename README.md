pintos-hkw
==========

CS124 Team Members:
- Tim Holland
- Daniel Kong
- Daniel Wang

About git:
The way I have my local repository set up, I have the submission repository (on the cms cluster) as 'cms' and the github repository as 'origin'. Running `git remote -v` yields this:

```
cms	dkong@login.cms.caltech.edu:/cs/courses/cs124/teams/pintos-hkw (fetch)
cms	dkong@login.cms.caltech.edu:/cs/courses/cs124/teams/pintos-hkw (push)
origin	https://github.com/dkong1796/pintos-hkw.git (fetch)
origin	https://github.com/dkong1796/pintos-hkw.git (push)
```

This way, running the usual commands like `git push origin master` update the github repository, and then when we want to submit, we (one of us) needs to run `git push cms master`.

To set it up, it's pretty simple. The easiest way to do it is to run these commands:

```
git clone https://github.com/dkong1796/pintos-hkw.git
git remote add cms [username]@login.cms.caltech.edu:/cs/courses/cs124/teams/pintos-hkw
```

Alternatively, if you already started with the cms repository, you can do this:

```
git remote rename origin cms
git remote add origin https://github.com/dkong1796/pintos-hkw.git
```
