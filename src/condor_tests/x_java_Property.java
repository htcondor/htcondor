
public class x_java_Property
{
	public static void main( String [] args )
	{
		int i = 0;
		System.out.println("Property Checking\n");
		//System.out.println("Args to print:" + args.length);
		while(i < args.length)
		{
			String prop = System.getProperty(args[i]);
			System.out.println(args[i] + " = " + prop);
			i++;
		}
		System.out.println();
	}
}
