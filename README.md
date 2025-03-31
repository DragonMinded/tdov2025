TDOV
====

A dummy simple scroller for the SEGA Naomi that displays a trans flag and a message for TDOV 2025. This is completely unoptimized and probably a bad example of how to do such a thing. Whatever. Ideally this would use the TA and hardware rendering for the font which would speed up rendering significantly. Run this on your Naomi by netbooting `tdov2025.bin` out of the `release/` directory.

Much of this code was ganked from xmplay and pared down so that I wouldn't have to write an audio integration. Therefore, the code for playing a file is more generic than it needs to be. Same with the font stuff. We never need more than one font, and we never need to play another tune. Again, whatever.

Recording of this running in Demul: [https://www.youtube.com/watch?v=r_Jj_a231OQ](https://www.youtube.com/watch?v=r_Jj_a231OQ)
