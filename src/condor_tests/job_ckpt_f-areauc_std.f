C This program computes the integral of a known function with known
C ranges and compares them against analytically derived answers. If the
C program determines that the computed value is not what you expected, then
C it will print out FAILURE, otherwise it will print out SUCCESS
C Written by psilord 08/08/2000

      PROGRAM AREAUC

      DOUBLE PRECISION XL(4), XR(4), IVL, AREA(4), RES(4)
      INTEGER I
      INTEGER SUCC
C Don't increase C above 4 because it converges faster than bits of precision
C in the real representation. You'll just end up introducing error into the
C system and getting bad numbers out. Also, IVL MUST be even for this adapted
C algorithm to work. 
      PARAMETER (IVL = 4)

C Assume the tests are going to succeed
      SUCC = 1

C A simple test set
      XL(1) = 0.0
      XR(1) = 1.0
      XL(2) = -1.0
      XR(2) = 0.0
      XL(3) = -1.0
      XR(3) = 1.0
      XL(4) = -1.0
      XR(4) = 7.0

C These are the results I should expect.
      RES(1) = 0.25000
      RES(2) = -0.2500
      RES(3) = 0.00000
      RES(4) = 600.00000

C Compute the tests for the ranges specified
       DO 42 I = 1, 4
          AREA(I) = SIMPSN(XL(I), XR(I), IVL)
C          PRINT '(A, F9.5, A, F9.5)', 'AREA UNDER CURVE IS ', AREA(I), 
C     +       ' EXPECTED: ', RES(I)
       CALL CKPT_AND_EXIT
   42  CONTINUE

C Compare the tests to see if all is ok
       DO 43 I = 1, 4
          IF (AREA(I) .NE. RES(I)) THEN
             SUCC = 0
          END IF
   43  CONTINUE

C Print out the results
       IF (SUCC .EQ. 1) THEN
          PRINT '(A)', 'SUCCESS'
       ELSE
          PRINT '(A)', 'FAILURE'
       END IF

       STOP
       END

C ----------------------------------------------------------------------

       REAL FUNCTION SIMPSN(A, B, N)
C Computes the area under x^3 from between A and B using Simpson's
C rule for N intervals
C Precondition: A must be less than B and N is defined
C Postcondition: returns the area under F(X) from X = A to X = B
C Adapted from Friedmann's fortran book page 641

C Input Arguments
C	A, B - the endpoints of the integration region
C	N - the number of intervals

C Argument Declarations
       DOUBLE PRECISION A, B, N

C Local Declarations
       DOUBLE PRECISION PI
       PARAMETER (PI = 3.14159)
       DOUBLE PRECISION H, DELTAZ, SUMODD, SUMEVE, Z
       INTEGER I

C Function I am integrating over...
       DOUBLE PRECISION F, X
       F(X) = X ** 3

C Compute required increments
       H = (B - A) / N
       DELTAZ = 2.0 * H

C Compute SUMODD
C This collects all of the odd coeffs into one sum
       SUMODD = 0.0
       Z = A + H
       DO 10 I = 2, N, 2
          SUMODD = SUMODD + F(Z)
          CALL CKPT_AND_EXIT
          Z = Z + DELTAZ
  10   CONTINUE

C Compute SUMEVE
C This collects all of the even coeffs into one sum
       SUMEVE = 0.0
       Z = A + DELTAZ
       DO 20 I = 2, N-2, 2
          SUMEVE = SUMEVE + F(Z)
          CALL CKPT_AND_EXIT
          Z = Z + DELTAZ
  20   CONTINUE

C Compute the integral and return
       SIMPSN = (F(A) + F(B) + 4.0 * SUMODD + 2.0 * SUMEVE) * (H / 3.0)
       RETURN
       END
