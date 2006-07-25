      LOGICAL EX,OP,NMD
      WRITE (*,*) ' Will open direct access file for reading'
      OPEN(11,FILE = 'x_hello.in',
     *     ACCESS = 'DIRECT',
     *     STATUS = 'OLD',FORM = 'UNFORMATTED',
     *     RECL = 1024,ERR = 900)
      INQUIRE(11,EXIST=EX,OPENED=OP,NAMED=NMD)
      close (11)
C----------------------------------------------------------------------
      Write(*,*) EX,OP,NMD
C-----------------------------------------------------------------------
      stop
900   write(*,*) 'error in opening file'
      END
