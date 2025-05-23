# lisa_utils
A collection of utility programs for interacting with the Apple Lisa's BLU and DC42 disk image formats.

---------- fixer ----------

`fixer.c` converts a 5MB Lisa ProFile image generated by BLU into a mountable/bootable Disk Copy 4.2 (`.dc42`) image for use in an emulator such as LisaEM.

To compile:
`gcc -o fixer fixer.c`

To run:
`./fixer`

Expected input is a BLU image titled `BLU.blu`.
Output is a DC42 image titled `ProFile.dc42`.

---------- lisa_password_generator ----------

Individual documents can be password protected on the Lisa. When the user enters a password, it is hashed and written to disk at a specific location.
This Python code is an attempt to reverse engineer this algorithm; it generates the password string for a given input plaintext password string.

---------- srcBuilder ----------

The utilities in this folder include ways to interact with 5MB Lisa DC42 disk images using the B-tree version of the filesystem.

wswrite.c writes files from the host machine to a disk image.
- Input files, for now, are configured manually by editing the `main` method. Be sure to set the corresponding boolean flags properly if you want to transfer Pascal source and if you want to transfer text files.
- The program expects the input disk image to be named WS_MASTER.dc42 in the current directory.
- The program outputs WS_new.dc42, containing the specified files, also into the current directory.

wsread.c extracts files from a specified disk image.
- The program expects the input disk image to be named WS_new.dc42 in the current directory.
- The program writes files into a folder at path `/extracted`.

To compile:
`gcc -o write wswrite.c`
`gcc -o write wsread.c`

To run:
`./write`
`./read`

Both are in progress and have potentially significant bugs. Use at your own peril!