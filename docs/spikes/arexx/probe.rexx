/* ARexx behaviour probe; argument: run id */
PARSE ARG run
OPTIONS RESULTS
out = 'RESULTS:probe-'run'-rexx.txt'
CALL lineout out, 'run' run 'version' PARSE_VERSION()
CALL w 'show REXX='SHOW('P','REXX') 'RXPROBE='SHOW('P','RXPROBE')
ADDRESS RXPROBE
'ECHO hello world'
CALL w 'echo1 rc='rc 'result=['result']'
'ECHO  two  spaces "dq" ''sq'' tab'||'09'x||'end'
CALL w 'echo2 rc='rc 'result=['result'] hex='c2x(result)
'NL'
CALL w 'nl rc='rc 'hex='c2x(result)
'EMPTY'
CALL w 'empty rc='rc 'len='length(result)
'LONG 5000'
CALL w 'long5000 rc='rc 'len='length(result)
'LONG 70000'
CALL w 'long70000 rc='rc 'len='length(result)
DROP result
'RCN 10 42'
CALL w 'rcn rc='rc 'rc2sym='symbol('RC2') 'resultsym='symbol('RESULT')
IF symbol('RC2')='VAR' THEN CALL w '  rc2=['rc2']'
result = 'before'
'RCN 5 7'
CALL w 'rcn5 rc='rc 'result=['result'] resultsym='symbol('RESULT')
'SETVAR PV some value here'
CALL w 'setvar rc='rc 'result=['result'] pvsym='symbol('PV')
IF symbol('PV')='VAR' THEN CALL w '  PV=['pv']'
'SETVAR STEM.3 stemvalue'
CALL w 'setstem rc='rc 'result=['result'] stem.3=['stem.3']'
'SETVARFAIL'
CALL w 'setvarfail rc='rc 'rc2sym='symbol('RC2')
IF symbol('RC2')='VAR' THEN CALL w '  rc2=['rc2']'
'NOSUCHCMD'
CALL w 'nosuch rc='rc
OPTIONS
'ECHO without results'
CALL w 'noresults rc='rc 'resultsym='symbol('RESULT')
OPTIONS RESULTS
ADDRESS NOSUCHPORT 'HELLO'
CALL w 'noport rc='rc
ADDRESS RXPROBE
'QUIT'
CALL w 'quit rc='rc 'result=['result']'
CALL w 'end'
EXIT 0
w: PROCEDURE EXPOSE out
  CALL lineout out, arg(1)
  RETURN
