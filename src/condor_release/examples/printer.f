      INTEGER DEBUGF
      COMMON/DEBUGF/DEBUGF

      INTEGER I, J
      INTEGER K

C
C     DebugFlags = D_ALL
C


      DO 100 I=0,1
          WRITE(*,*)'set i =', I

          DO 90 J=0,99999
              K = J
              IF (J .NE. K) THEN
                  WRITE(*,*)'J =', J, ', K =', K
              ENDIF
90        CONTINUE

          CALL CKPT

100   CONTINUE
      CALL EXIT(0)

200   WRITE(*,*)'Error opening results'
      CALL EXIT(1)
      END
