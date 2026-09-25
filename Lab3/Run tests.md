Run this to test
```sh
    cp -r ~/os_labs/Lab3/pintos ~/

    export PATH=/chalmers/sw/unsup64/phc/b/pkg/bochs-2.6.6/bin:$HOME/pintos/src/utils:$PATH

    source $HOME/.bashrc

    chmod +x pintos/src/utils/pintos*
    chmod +x pintos/src/utils/backtrace

    cd ~/os_labs/Lab3/pintos/src/threads/build
    make check

```