/* rx crash2.rexx <run> <case>: the harness's calling pattern, repeated */
PARSE ARG run case .
OPTIONS RESULTS
out = 'RESULTS:'run'-crash2-'case'.txt'
CALL lineout out, 'start' case
DO i = 1 TO 40
  SELECT
    WHEN case = 'procdrop'   THEN r = askdrop('INFO nosuch')
    WHEN case = 'procnodrop' THEN r = asknodrop('INFO nosuch')
    WHEN case = 'procmixed'  THEN DO; r = askdrop('INFO nosuch'); r = askdrop('VERSION'); END
    WHEN case = 'harness'    THEN DO; r = askdrop('FROB'); r = askdrop('INFO "a*"b;$x&y"'); r = askdrop('INFO soliton'); END
    OTHERWISE NOP
  END
  IF i // 10 = 0 THEN CALL lineout out, i r
END
CALL lineout out, 'end' case
CALL lineout out
EXIT 0

askdrop: PROCEDURE EXPOSE out
  PARSE ARG line
  DROP result rc2
  ADDRESS PKGMANAGER line
  IF symbol('RESULT') = 'VAR' THEN RETURN 'ok' length(result)
  IF symbol('RC2') = 'VAR' THEN RETURN 'err' word(rc2, 1)
  RETURN 'noreply'

asknodrop: PROCEDURE EXPOSE out
  PARSE ARG line
  ADDRESS PKGMANAGER line
  IF symbol('RESULT') = 'VAR' THEN RETURN 'ok' length(result)
  IF symbol('RC2') = 'VAR' THEN RETURN 'err' word(rc2, 1)
  RETURN 'noreply'
