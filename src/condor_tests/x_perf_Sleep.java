
import java.util.*;

public class x_perf_Sleep
{
	public static void main( String [] args )
	    throws Exception
	{
	    //for (int i = 0; i < 5; i++)
		{
		    System.out.println("tick!");
		    Thread.sleep(Integer.parseInt(args[0]) * 1000);
		}
	}

}
