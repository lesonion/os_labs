Run this to test
```sh
    cp -r ~/os_labs/Lab2/pintos ~/

    export PATH=/chalmers/sw/unsup64/phc/b/pkg/bochs-2.6.6/bin:$HOME/pintos/src/utils:$PATH

    source $HOME/.bashrc

    chmod +x pintos/src/utils/pintos*
    chmod +x pintos/src/utils/backtrace

    cd pintos/src/threads/build
    make check

```