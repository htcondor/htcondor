import java.io.*;

public class x_java_ReadStdin
{
	public static int stdinwaiting;
	public static String oneline;
	public static void main( String [] args )
	{
		BufferedReader in = new BufferedReader (new InputStreamReader(System.in));
		try
		{
			while( (oneline = in.readLine()) != null)
			{
				System.out.println( oneline );
			}
		}
		catch ( IOException e )
		{
			System.out.println("Oops.....");
		}
	}
}
