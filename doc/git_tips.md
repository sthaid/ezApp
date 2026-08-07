GIT TIPS
========

Cloning
=======

ssh cloning
- git clone git@github.com:sthaid/ezApp.git
- Streamlined workflow, github account with ssh key, is required. 
  See section below on adding your public ssh key to your github account.

https cloning
- git clone https://github.com/sthaid/ezApp.git
- No git account needed.

Diff
====

git diff

Branches
========

Create a feature-branch:
- git checkout main
- git pull
- git switch -c <feature-branch>
- git push -u origin <feature-branch>

Merge feature-branch back to main;
- may want to first 'git rebase -i' on the feature-branch:
- git checkout main
- git pull
- git merge <feature-branch>
- git push

Change branch:
- git checkout <branch>

List all branches:
- git branch --all

Commit Fixup
============

Combine sequence of commits, use this only on a personal feature-branch
- git rebase -i <commit-hash>  : will bring up editor, from which you can
  customize the rebase operaton; some of the choices:
  - 'fixup'  : melds the commit into the previous commit
  - 'reword' : edit the commit message
- git push --force

Bash Prompt
===========

Set bash prompt to include branch name, add this to your .bashrc file:
  parse_git_branch() {
     git branch --show-current 2> /dev/null | sed 's/\(.*\)/ (\1)/'
  }
  export PS1="\u@\h \[\033[32m\]\w\[\033[33m\]\$(parse_git_branch)\[\033[00m\] \$ "

Add Your Public SSH Key to Your Github Account
==============================================

- Log into your github account and click your profile photo in the top-right corner.
- Select Settings.
- In the left sidebar, click SSH and GPG keys.
- Click the green New SSH key button.
- In the Title field, type a descriptive label.
- Leave the Key type as Authentication Key.
- Paste your text into the Key field and click Add SSH key.

Workflow Example
================

Clone repo using ssh, cloning with ssh is required if you plan to make changes to origin.
- git clone git@github.com:sthaid/ezApp.git

Make change to file(s).
- vi ezApp/src/main.c

List the files that have been modified.
- git status

xxx try alternate method
View the diff of all modified files;
GIT_EXTERNAL_DIFF env var can opptionally be set to specify an alternate diff program;
the ezApp repo uses GIT_EXTERNAL_DIFF to view diff using the meld program
- git diff

Commit the changes; this commits the changes on your Devel PC.
The changes will not yet be copied back to the origin (github) repo.
- git commit --all

View the commit history.
- git log

Copy changes back to the origin repo, on github.
This will fail unless you are the owner of the repo, 
or have been added by the owner as a collaborator.
- git push

If you now want to pull in other developer's changes, that they had pushed to the origin:
- git pull
- git log 
