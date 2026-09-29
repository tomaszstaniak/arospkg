/* Which reply takes RexxMast down: rx crash.rexx <run> <case> */
PARSE ARG run case .
OPTIONS RESULTS
out = 'RESULTS:'run'-crash-'case'.txt'
CALL lineout out, 'start' case
SELECT
  WHEN case = 'many' THEN DO i = 1 TO 50
    DROP rc2
    ADDRESS PKGMANAGER 'INFO nosuch'
    IF i // 10 = 0 THEN CALL lineout out, i rc2
  END
  WHEN case = 'quote' THEN ADDRESS PKGMANAGER 'INFO "a*"b"'
  WHEN case = 'semi'  THEN ADDRESS PKGMANAGER 'INFO "a;b"'
  WHEN case = 'dollar' THEN ADDRESS PKGMANAGER 'INFO "a$b"'
  WHEN case = 'amp'   THEN ADDRESS PKGMANAGER 'INFO "a&b"'
  WHEN case = 'all'   THEN ADDRESS PKGMANAGER 'INFO "a*"b;$x&y"'
  WHEN case = 'okmany' THEN DO i = 1 TO 50
    ADDRESS PKGMANAGER 'VERSION'
  END
  OTHERWISE NOP
END
IF symbol('RC2') = 'VAR' THEN CALL lineout out, 'rc2 ['rc2']'
CALL lineout out, 'end' case
CALL lineout out
EXIT 0
