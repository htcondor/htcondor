/*
 * IOUtils.java
 *
 * Created on November 3, 2003, 6:40 PM
 */

package condor.gahp;


import java.io.File;
import java.io.FileInputStream;

/**
 *
 * @author  ckireyev
 */
public class IOUtils {
    public static String escapeWord (String word) {
        if (word == null) return null;
    
        StringBuffer result = new StringBuffer();
        for (int i=0; i<word.length(); i++) {
            char c = word.charAt(i);
            if (c == ' ' || c == '\\' ||c == '\r' || c == '\n') {
                result.append ('\\');
            }
            result.append (c);
        }
        return result.toString();
    }
    
	public static void main(String[] args) throws Exception {
		byte[] buffer = new byte[10000];
		File file = new File (args[0]);
		FileInputStream stream = new FileInputStream(file);
		stream.read (buffer);
		stream.close();

		System.out.println (escapeWord (new String(buffer)));

		
	}
}
