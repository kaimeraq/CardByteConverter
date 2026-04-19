This is a simple 'byte converter' tool for card data that I've created in order to assist with encoding Unicodes into tokenized ASCII characters for my game Card Roulette's console software renderer. This will eventually be incorporated directly into the engine but I wanted to keep a standalone tool available in C as well.

The file `cards.conf` is a custom config file that I created based on .conf standards which contains the unicode data and deck config.

The tool outputs to `cards.dat` which I've included for demonstration purposes.

Note that this depends on libiconv which I've linked as a submodule, for this code to work you will need to link `libiconv*.dll` and include `iconv.h` which I purposely left out of my repo.

I've tested it with cc on Windows 11 with the command: (Note that you need to place the iconv header and dll in the same folder as encode.c for this to work)

`cc encode.c -o encode -L. -liconvD`

All my code provided "AS IS" and is subject to the MIT license.<br>
`libiconv` is linked under [LGPL3.0][1].

[1]: https://www.gnu.org/licenses/lgpl.html
