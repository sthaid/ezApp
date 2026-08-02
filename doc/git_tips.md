GIT TIPS
========

Cloning
=======

ssh cloning
- git clone git@github.com:sthaid/ezApp.git
- streamlined workflow
- github account with ssh key, is required. 
- refer to section below on adding your public ssh key to your github account.

https cloning
- git clone https://github.com/sthaid/ezApp.git
- No git account needed.

Diff
====

git diff

Diff has been configured to generate the diff using meld.
The setup_devel_pc script has configured git using command:
- git config --global diff.external ~/ezApp/bin/git_meld_diff

Branches
========

Create a branch:
- git checkout main
- git pull
- git switch -c <branch>        # create branch and switch to it
- git push -u origin <branch>   # make the branch visible at the origin

Merge branch back to main;
- may want to first 'git rebase -i' on the branch:
- git checkout main
- git pull
- git merge <branch>
- git push

Switch to an existing branch:
- git switch <branch>

List all branches:
- git branch --all

Commit Fixup
============

Combine sequence of commits, use this only when working on a personal branch
- git rebase -i <commit-hash>  : will bring up editor, from which you can
  customize the rebase operation; some of the choices:
  - 'fixup'  : melds the commit into the previous commit
  - 'reword' : edit the commit message
- git push --force    # force push is required following git rebase

Bash Prompt
===========

Set bash prompt to include branch name, add this to your .bashrc file:
```
parse_git_branch() {
   git branch --show-current 2> /dev/null | sed 's/\(.*\)/ (\1)/'
}
export PS1="\u@\h \[\033[32m\]\w\[\033[33m\]\$(parse_git_branch)\[\033[00m\] \$ "
```

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

View the diff of all modified files;
- git diff

Commit the changes; this commits the changes on your Devel PC copy of repo.
The changes will not yet be copied back to the origin (github) repo.
- git commit --all

View the commit history.
- git log

Copy changes back to the origin repo, on github.
This will fail unless you are the owner of the repo, 
or have been added by the owner as a collaborator.
- git push

To get changes that have been made, and pushed, by other developers:
- git pull
- git log 
