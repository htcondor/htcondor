      INTEGER DEBUGF
      COMMON/DEBUGF/DEBUGF
      CHARACTER*1 A

C     READ(*,'(A)',END=100) A
C     DEBUGF = -1

100   CALL FOO(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
      CALL EXIT(0)
      END


      SUBROUTINE FOO(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7)
      REAL ARG0
      REAL ARG1
      REAL ARG2
      REAL ARG3
      REAL ARG4
      REAL ARG5
      REAL ARG6
      REAL ARG7
      REAL LOCAL0
      REAL LOCAL1
      REAL LOCAL2
      REAL LOCAL3
      REAL LOCAL4
      REAL LOCAL5
      REAL LOCAL6
      REAL LOCAL7

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
