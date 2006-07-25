      INTEGER DEBUGF
      COMMON/DEBUGF/DEBUGF
      CHARACTER*1 A

C     READ(*,'(A)',END=100) A
C     DEBUGF = -1

100   CALL FOO(0, 0, 0, 0, 0, 0, 0, 0)
      CALL EXIT(0)
      END


      SUBROUTINE FOO(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)
      INTEGER ARG0
      INTEGER ARG1
      INTEGER ARG2
      INTEGER ARG3
      INTEGER ARG4
      INTEGER ARG5
      INTEGER ARG6
      INTEGER ARG7
      INTEGER LOCAL0
      INTEGER LOCAL1
      INTEGER LOCAL2
      INTEGER LOCAL3
      INTEGER LOCAL4
      INTEGER LOCAL5
      INTEGER LOCAL6
      INTEGER LOCAL7

      IF (ARG0 .GT. 34) THEN
          WRITE(*,*)'About to checkpoint'
          CALL CKPT_AND_EXIT
          WRITE(*,*)'Returned from checkpoint'
          WRITE(*,*)'args:', ARG0, ARG1, ARG2, ARG3,
     +                       ARG4, ARG5, ARG6, ARG7
          RETURN
      ENDIF

      LOCAL0 = -(ARG0 +1)
      LOCAL1 = -(ARG1 +1)
      LOCAL2 = -(ARG2 +1)
      LOCAL3 = -(ARG3 +1)
      LOCAL4 = -(ARG4 +1)
      LOCAL5 = -(ARG5 +1)
      LOCAL6 = -(ARG6 +1)
      LOCAL7 = -(ARG7 +1)
      CALL FOO(-LOCAL0, -LOCAL1, -LOCAL2, -LOCAL3,
     +         -LOCAL4, -LOCAL5, -LOCAL6, -LOCAL7)

      WRITE(*,*)'args:', ARG0, ARG1, ARG2, ARG3,
     +                   ARG4, ARG5, ARG6, ARG7

      WRITE(*,*)'locals:', LOCAL0, LOCAL1, LOCAL2, LOCAL3,
     +                     LOCAL4, LOCAL5, LOCAL6, LOCAL7
      END
