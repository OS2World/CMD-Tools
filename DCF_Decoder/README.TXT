DCF Decoder for radioclock receiver ver.:1.0.1 OS/2 (Jan 22 2000)
=================================================================
- usage:  dcf [-p#] [-n#] [-s#]
          p...com port base address (hex) (default 0x2f8)
              standard com1-4 :  0x3f8,0x2f8,0x3e8,0x2e8
          n...requiered sucessfull blocks (default     2)
          s...sleep time [min]            (default    15)
  sample: dcf -p3f8 -n3 -s30

- required: emx runtime

- compiled for serial port, power on RTS, signal on DSR
- for other hardware change macros INIT (power) and POOL (signal)
  in source code (not tested)

history:
 Jan 01 2000 : ver:1.0
 Jan 22 2000 : ver:1.0.1 fixed day decoding bug 

B.Horeni horeni@login.cz

DCF signal sample ( BCD )
=========================

LSB                                                     MSB
..................#.#....#..##..#...#......###.............
..................#.##...#...#..#...#......###.............
..................#.#.#..#...#..#...#......###.............
                 || |-------|------|------***-----********|
                 || |  min  P   h  P   d   w  mon   year  P
                 || |                                    
                 || |  12       9      1   6   1     0 
                 || |                      |   |     |
                 || |                      |   |     |
                 || |                      |   |     |
                 || +-- startbit (1)       |   |     +-- 2000
                 |+---- wintertime         |   +-------- January 
                 +----- summertime         +------------ Saturday

