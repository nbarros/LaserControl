# LaserControl

Library for control of serial devices in the DUNE IOLaser system. This library is a direct port of the `controlLibrary` python module implemented by Eric Deck:

https://gitlab.com/ericemersondeck/protodune-iolaser-control/-/tree/main/controlLibrary

## Building instructions

All the dependencies of this library are self-contained. In fact, there is only a single dependency ([Serial Communication Library](https://github.com/wjwwood/serial)), whose source I copied into this repository (eventually I will set things up as a git submodule, but for now it is simpler this way).

The code will likely suffer some modifications (mostly in the exception handling department). 

The library is built using `camke`:

1. Create a building folder : `mkdir build && cd build`
2. Call cmake : `cmake ../`
3. Build : `make`

At this point you should have a library called `libLaserControl.a` that you can link into your application.

## TODO

[ ] Add python bindings
[ ] Add client that will permit a remote GUI to connect and act
[ ] Integrate into CIB quasar server

## Sync on gitlba

https://dev.to/brunorobert/github-and-gitlab-sync-44mn
 
