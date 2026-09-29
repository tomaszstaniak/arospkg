/* ARexx probe 2: what RC and RESULT hold right after a command */
PARSE ARG run .
OPTIONS RESULTS
out = 'RESULTS:probe-'run'-rexx2.txt'
PARSE VERSION v
PARSE SOURCE s
CALL lineout out, 'run' run 'version' v
CALL lineout out, 'source' s
ADDRESS RXPROBE
'ECHO hello world'
  r = rc; x = result; CALL w 'echo' r x
'RCN 10 42'
  r = rc; x = symbol('RESULT'); y = symbol('RC2'); CALL w 'rcn10' r x y
'RCN 5 7'
  r = rc; x = symbol('RESULT'); CALL w 'rcn5' r x
'NOSUCHCMD'
  r = rc; CALL w 'nosuch' r
'ECHO 0'
  r = rc; x = result; CALL w 'echo0' r x
'SETVARFAIL'
  r = rc; y = symbol('RC2'); z = ''; IF y = 'VAR' THEN z = rc2; CALL w 'setvarfail' r y z
ADDRESS NOSUCHPORT 'HELLO'
  r = rc; CALL w 'noport' r
ADDRESS RXPROBE
OPTIONS
'ECHO nores'
  r = rc; x = symbol('RESULT'); CALL w 'nores' r x
OPTIONS RESULTS
SIGNAL ON ERROR
'RCN 10 1'
CALL w 'not reached'
EXIT 0
ERROR:
  r = rc; CALL w 'error-trap sigl='sigl 'rc='r
  ADDRESS RXPROBE 'QUIT'
  CALL w 'end'
  EXIT 0
w: PROCEDURE EXPOSE out
  PARSE ARG tag r x y z
  CALL lineout out, tag 'rc=[' || r || '] rest=[' || x || '|' || y || '|' || z || ']'
  RETURN
