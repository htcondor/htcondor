! This program computes the integral of a known function with known
! ranges and compares them against analytically derived answers. If the
! program determines that the computed value is not what you expected, then
! it will print out FAILURE, otherwise it will print out SUCCESS
! Written by psilord 01/19/2008, converted from f77 into f90.

PROGRAM AREAUC
	IMPLICIT NONE

	REAL, DIMENSION(1:4) :: XL, XR, AREA, RES

	INTEGER :: I
	INTEGER :: SUCC

	! Don't increase C above 4 because it converges faster than bits of
	! precision in the real representation. You'll just end up introducing
	! error into the system and getting bad numbers out. Also, IVL MUST be even
	! for this adapted algorithm to work. 

	REAL, PARAMETER :: IVL = 4

	! Assume the tests are going to succeed
	SUCC = 1

	! A simple test set
	XL(1) = 0.0
	XR(1) = 1.0
	XL(2) = -1.0
	XR(2) = 0.0
	XL(3) = -1.0
	XR(3) = 1.0
	XL(4) = -1.0
	XR(4) = 7.0

	! These are the results I should expect gotten from analytical analysis.
	RES(1) = 0.25000
	RES(2) = -0.2500
	RES(3) = 0.00000
	RES(4) = 600.00000

	! Compute the tests for the ranges specified
	DO I = 1, 4
		AREA(I) = SIMPSN(XL(I), XR(I), IVL)
		!PRINT '(A, F9.5, A, F9.5)', 'AREA UNDER CURVE IS ', AREA(I), &
      	!	 ' EXPECTED: ', RES(I)
	CALL CKPT_AND_EXIT
	END DO

	! Compare the tests to see if all is ok
	DO I = 1, 4
		IF (AREA(I) .NE. RES(I)) THEN
			SUCC = 0
		END IF
	END DO

	! Print out the results
	IF (SUCC .EQ. 1) THEN
		PRINT '(A)', 'SUCCESS'
	ELSE
		PRINT '(A)', 'FAILURE'
	END IF

CONTAINS
! ----------------------------------------------------------------------
! and now all of my function definitions.
! ----------------------------------------------------------------------

! The one dimensional function I wish to integrate: f(x) = x^3
REAL FUNCTION F(X)
	IMPLICIT NONE
	REAL, INTENT(IN) :: X

	F = X ** 3
END FUNCTION F

! Computes the area under x^3 from between A and B using Simpson's
! rule for N intervals
! Precondition: A must be less than B and N is defined
! Postcondition: returns the area under F(X) from X = A to X = B
! Adapted from Friedmann's fortran book page 641
! Input Arguments
!	A, B - the endpoints of the integration region
!	N - the number of intervals
REAL FUNCTION SIMPSN(A, B, N)
	IMPLICIT NONE

	! Argument Declarations
	REAL, INTENT(IN) :: A, B, N

	! Local Declarations
	REAL, PARAMETER :: PI = 3.14159
	REAL :: H, DELTAZ, SUMODD, SUMEVE, Z
	INTEGER :: I

	! Compute required increments
	H = (B - A) / N
	DELTAZ = 2.0 * H

	! Compute SUMODD
	! This collects all of the odd coeffs into one sum
	SUMODD = 0.0
	Z = A + H
	DO I = 2, N, 2
		SUMODD = SUMODD + F(Z)
		CALL CKPT_AND_EXIT
		Z = Z + DELTAZ
	END DO

	! Compute SUMEVE
	! This collects all of the even coeffs into one sum
	SUMEVE = 0.0
	Z = A + DELTAZ
	DO I = 2, N-2, 2
		SUMEVE = SUMEVE + F(Z)
		CALL CKPT_AND_EXIT
		Z = Z + DELTAZ
	END DO

	! Compute the integral and return
	SIMPSN = (F(A) + F(B) + 4.0 * SUMODD + 2.0 * SUMEVE) * (H / 3.0)
END FUNCTION SIMPSN

END PROGRAM AREAUC
