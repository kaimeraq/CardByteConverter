This is a simple 'byte converter' tool for card data that I've created in order to assist with encoding Unicodes into tokenized ASCII characters for my game Card Roulette's console software renderer. This will eventually be incorporated directly into the engine but I wanted to keep a standalone tool available in C as well.

Note that this depends on libiconv which I've linked as a submodule, for this code to work you will need to link `libiconv*.dll` and include `iconv.h` which I purposely left out of my repo.

All code provided "AS IS" and is subject to the MIT license.
