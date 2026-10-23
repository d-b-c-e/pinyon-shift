# Third-party notices

Pinyon Shift downloads the following third-party projects during local setup.
Their own licenses apply; they are not relicensed by this repository.

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)
- [LLVM](https://github.com/llvm/llvm-project)
- [extract-xiso](https://github.com/XboxDev/extract-xiso)
- [Git for Windows](https://github.com/git-for-windows/git)
- [Microsoft Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)

Exact versions, source URLs, and archive hashes are recorded in
`config/release-toolchain.json`. ReXGlue's transitive dependencies are fetched
by its pinned source tree and retain their upstream notices.

The private Wheelkit adapter includes DBCE's shared input shim from toolkit
commit `ee8014d4ca7f3b38604b0fcc2baef460aabf7021` under the MIT license.
Its exact source closure, license and provenance are in `thirdparty/dbce-shim/`.
This source pin does not enable the adapter in ordinary builds.
