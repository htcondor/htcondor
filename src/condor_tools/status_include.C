#define _POSIX_SOURCE
#if defined(Solaris)
#include "_condor_fix_types.h"
#endif
#include "status_include.h"

#if defined(Solaris) || defined(IRIX53) || defined(OSF1) || defined(LINUX) || defined (HPUX9)
void SERVER_REC::print_rec( FILE *fp )
#else
virtual void SERVER_REC::print_rec( FILE *fp )
#endif
{	
	fprintf( fp, "%-14s", this->name );

 	if(!AvailOnly)
	{
	  if( this->idle ) {
		fprintf( fp, "%-5s ", "True" );
	  } else {
		fprintf( fp, "%-5s ", "False" );
	  }
   	}

	if( this->swap < 0.0 ) {
		fprintf( fp, "%-6s ", "?" );
	} else {
		fprintf( fp, "%-6.1f ", this->swap );
	}

	if( this->disk < 0.0 ) {
		fprintf( fp, "%-6s ", "?" );
	} else {
		fprintf( fp, "%-6.1f ", this->disk );
	}

	if(!AvailOnly)
	  fprintf( fp, "%-6s ", this->state );

	if( this->load_avg < 0.0 ) {
		fprintf( fp, "%-6s", "?" );
	} else {
		fprintf( fp, "%-6.2f", this->load_avg );
	}

	if( this->kbd_idle < 0 ) {
		fprintf( fp, "%-12s ", "?" );
	} else {
		fprintf( fp, "%12s ", format_seconds(this->kbd_idle) );
	}
	
	if(AvailOnly) {
	  if( this->bench_mips <= 0 ) {
	      fprintf( fp, "%3s  ", "?" );
	  } else {
	      fprintf( fp, "%3d ", this->bench_mips );
	  }
	  
	  if( this->kflops <= 0 ) {
	       fprintf( fp, "%-5s ", "?" );
	  } else {
	       fprintf( fp, "%-6.2f ", (float)this->kflops/1000 );
	  }
	}

	fprintf( fp, "%-7s", this->arch );
	fprintf( fp, "%-8s ", this->op_sys );

	fprintf( fp, "\n" );
}





	

void SUBMITTOR_REC::print_rec( FILE *fp, int counted )
{
	
	fprintf( fp, "%-14s ", this->name );
        fprintf( fp, "%-4d ", counted );  // placeholder for now.
	fprintf( fp, "%-4d ", this->run );
	fprintf( fp, "%-4d ", this->tot );
	fprintf( fp, "%-4d ", this->max );
	fprintf( fp, "%-5d ", this->users );

	fprintf( fp, "%-9d ", this->prio );

	if( this->swap < 0.0 ) {
		fprintf( fp, "%-6s ", "?" );
	} else {
		fprintf( fp, "%-6.1f ", this->swap );
	}

	fprintf( fp, "%-7s ", this->arch );
	fprintf( fp, "%-8s ", this->op_sys );

	fprintf( fp, "\n" );
}






void RUN_REC::print_rec( FILE *fp )
{
        fprintf( fp, "%-14s", this->remote_user );
	fprintf( fp, "%-14s", this->name );
        fprintf( fp, "%-12s", this->state );

	if( this->load_avg < 0.0 ) {
		fprintf( fp, "%-7s", "?" );
	} else {
		fprintf( fp, "%-7.2f", this->load_avg );
	}

        fprintf( fp, "%-11s", this->client);
	fprintf( fp, "%-8s", this->arch );
	fprintf( fp, "%-10s", this->op_sys );
        fprintf( fp, "%12s\n", format_seconds(this->running) );

}




void JOB_REC::print_rec( FILE *fp )
{
        fprintf( fp, "%-14s", this->remote_user);   //this will add the owner.
        fprintf( fp, "%-11s", this->client);
	fprintf( fp, "%-14s\n", this->name );


}





/*
FOR NOW THIS DOES NOT SEEM TO WORK IN THE QSORT FUNCTION
SO I WILL LEAVE THIS HERE, SO IT DOES ON SEEM EASY TO PUT
THE COMPARE FUNCTIONS INTO THE CLASSES.
*/
int SUBMITTOR_REC::sub_comp( SUBMITTOR_REC *ptr2 )
{
	int		status;

	if( SortByPrio ) {
		if( status = ptr2->prio - this->prio ) {
			return status;
		}

		return strcmp( this->name, ptr2->name );
	} else {

		if( status = strcmp( this->arch, ptr2->arch ) ) {
			return status;
		}

		if( status = strcmp( this->op_sys, ptr2->op_sys ) ) {
			return status;
		}

		return strcmp( this->name, ptr2->name );
	}
}
