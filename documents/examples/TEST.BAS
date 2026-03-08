10 REM =====================================================
20 REM  Enhanced Micro-Basic 2.2 - operator test
30 REM  Tests: # @ literals, << >>, UGT(), ULT()
40 REM =====================================================
50 REM
60 REM --- Literal input ---
70 LET A = #FF
80 PRINT "HEX$(#FF)    = "; HEX$(A)
90 PRINT "UNS$(#FF)    = "; UNS$(A)
100 PRINT "STR$(#FF)    = "; STR$(A)
110 LET B = @255
120 PRINT "A=#FF B=@255 equal = "; STR$(A = B)
130 LET C = #8000
140 PRINT "HEX$(#8000)  = "; HEX$(C)
150 PRINT "STR$(#8000)  = "; STR$(C)
160 PRINT "UNS$(#8000)  = "; UNS$(C)
170 LET D = @65535
180 PRINT "UNS$(@65535) = "; UNS$(D)
190 PRINT "STR$(@65535) = "; STR$(D)
200 REM
210 REM --- Shift operators ---
220 LET A = 1
230 PRINT "1 << 8       = "; UNS$(A << 8)
240 LET A = #FF00
250 PRINT "#FF00 >> 8   = "; HEX$(A >> 8)
260 LET A = #FFFF
270 PRINT "#FFFF >> 1   = "; HEX$(A >> 1)
280 PRINT "#FFFF << 1   = "; HEX$(A << 1)
290 REM
300 REM --- Signed comparison pitfall ---
310 LET A = @65535
320 LET B = 0
330 PRINT "signed  65535 > 0     = "; STR$(A > B)
340 PRINT "UGT(@65535, 0)        = "; STR$(UGT(A, B))
350 PRINT "ULT(0, @65535)        = "; STR$(ULT(B, A))
360 REM
370 REM --- UGT / ULT edge cases ---
380 LET A = #8000
390 LET B = #7FFF
400 PRINT "signed  #8000 > #7FFF = "; STR$(A > B)
410 PRINT "UGT(#8000, #7FFF)     = "; STR$(UGT(A, B))
420 PRINT "ULT(#7FFF, #8000)     = "; STR$(ULT(B, A))
430 REM
440 REM --- Equality unaffected by sign ---
450 LET A = #FFFF
460 LET B = @65535
470 PRINT "#FFFF = @65535        = "; STR$(A = B)
480 REM
490 PRINT "done"
500 END
