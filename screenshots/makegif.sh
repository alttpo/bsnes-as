ffmpeg -ss 40.0 -t 1.8 -i Screen Recording 2019-09-15 at 3.21.16 PM.mov -filter_complex [0:v] fps=30,scale=-1:246,split [a][b];[a] palettegen [p];[b][p] paletteuse test.gif
