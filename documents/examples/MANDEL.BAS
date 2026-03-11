1 REM =====================================================
2 REM Mandelbrot - Q8 fixed point, Enhanced Micro-Basic 2.2
3 REM Q8: 1.0=256  2.0=512  -2.5=-640
4 REM Escape: |z|^2 > 4  =>  P+Q > 1024
5 REM =====================================================
20 W=59:H=22:M=16
30 A=-640:B=256:E=-275:F=275
40 U=(B-A)/W
50 V=(F-E)/H
60 FOR K=0 TO H
70   D=F-(K*V)
80   FOR J=0 TO W
90     C=A+(J*U)
100    X=0:Y=0:I=0
110    G=X:N=X:GOSUB 1000:P=R
120    G=Y:N=Y:GOSUB 1000:Q=R
130    IF I>=M THEN +8            :REM WHILE... DO loop  A
140    IF (P+Q)>1024 THEN +7      :REM WHILE... DO loop  B
150    T=P-Q+C
160    G=X:N=Y:GOSUB 1000
170    Y=R+R+D
180    X=T
190    I=I+1
200    GOTO 110
210    O=I&7                      :REM END BOTH WHILE... Loops
215    REM *** This is simalar to a switch() case
220    IF O>0 THEN +2
230    PRINT " ";:GOTO 390
240    IF O>1 THEN +2
250    PRINT ".";:GOTO 390
260    IF O>2 THEN +2
270    PRINT ":";:GOTO 390
280    IF O>3 THEN +2
290    PRINT "-";:GOTO 390
300    IF O>4 THEN +2
310    PRINT "+";:GOTO 390
320    IF O>5 THEN +2
330    PRINT "*";:GOTO 390
340    IF O>6 THEN +2
350    PRINT "#";:GOTO 390
360    PRINT "@";
390  NEXT J                          : REM SWITCH CASE EXITS HERE, GO TO NEXT J IN FOR LOOP
400  PRINT ""
410 NEXT K
420 PRINT "Done. ";W;"x";H;" M=";M
430 END
435 REM *** Fixed Point Math routines for 8.8 Fixed point
440 REM =====================================================
450 REM fp_mul: R = (G*N)>>8  signed Q8 multiply
460 REM In:  G=a, N=b    Out: R = (a*b)>>8
470 REM Uses: S (sign), L (|G|), Z (|N|), R (temp al*bl)
480 REM Does NOT touch: A B C D E F I J K M O P Q T U V W X Y
490 REM =====================================================
1000 S=(G<0)<>(N<0)  :REM After here we look at numbers as unsigned, we just need to know the sign for the end result
1010 L=ABS(G)
1020 Z=ABS(N)
1030 R=(L&255)*(Z&255)
1040 R=(L>>8)*(Z>>8)*256+(L>>8)*(Z&255)+(L&255)*(Z>>8)+(R>>8)
1050 IF S THEN R=-R
1060 RETURN
