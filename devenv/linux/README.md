# How to setup virtual development environment

By the following procedure, you can setup your virtual development environment.
You can apply the following procedure for Linux, MacOSX and Windows.


## Prerequisite

This procedure needs the following softwares:

  - [git](https://git-scm.com/)
  - [VirtualBox](https://www.virtualbox.org/)
  - [Vagrant](https://www.vagrantup.com/)


## Setup (Windows)

See [README-WINDOWS.md](README-WINDOWS.md)


## Setup (Linux and MacOSX)

Clone the xxHash repository

```
cd
git clone https://github.com/Cyan4973/xxHash.git xxhash
```

Setup and test virtual environemnt

```
cd
cd xxhash/devenv/linux
vagrant up
vagrant ssh

## If you see passphrase prompt, just press Enter ##
## If you see password prompt for vagrant, input "vagrant" as password ##
## here, you'll see GUEST terminal ##
# note: GUEST's ~/xxhash/ is mapped to HOST's xxhash/devenv/linux/xxhash/ #
cd xxhash
git checkout dev
make all
./xxhsum --help
make test-all
exit
## here, you'll back to the HOST ##

vagrant halt     # halt VM #
vagrant status   # show VM status #
```

After that, if you don't need virtual environemnt anymore, destory VM image by the following command:

```
cd
cd xxhash/devenv/linux
vagrant destroy  # delete VM image, if you want #
rm -rf xxhash    # delete cloned repository #
```
