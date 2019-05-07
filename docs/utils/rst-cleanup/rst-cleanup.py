#!/usr/bin/env python

import re
import sys

def main():

    # Command line arguments
    if len(sys.argv) < 2:
        print("Usage: rst-cleanup.py <input-file> [Optional]: <output-file>")
        sys.exit(1)
    
    # Open the input file
    input_filename = str(sys.argv[1])
    with open(input_filename, "r") as input_file:
        rst_data = input_file.read()
    input_file.close()

    # Check if output file was specified, if so record it
    output_filename = input_filename + ".out"
    if len(sys.argv) >= 3:
        output_filename = str(sys.argv[2])

    print("Cleaning up " + input_filename + " to " + output_filename)

    # Now start the actual parse!

    # Replace all our fake <index://...> tags and replace them with correct index roles.
    # Use a blank title for all auto-generated indices.
    rst_data = re.sub(r'%20', r' ', rst_data)
    rst_data = re.sub(r'`[\s]*<index://([\w\s\.\(\)\[\]\*\\\/\-=,<>:$%]*)>[\s]*`[_]*', r':index:`\1`', rst_data)
    rst_data = re.sub(r'`[\s]*<index://([\w\s\.\(\)\[\]\*\\\/\-=,<>:$%]*);([\w\s\.\(\)\[\]\*\\\/\-=,<>:$%]*)>[\s]*`[_]*', r':index:`\1<single: \1; \2>`', rst_data)

    # Replace all our ticket links with :ticket:`####` syntax.
    rst_data = re.sub(r'`\(Ticket[\s]*#([\d]*)\). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview\?tn=([\d]*)>`__', r':ticket:`\1`', rst_data)

    # Clean up instances of the following error:
    # WARNING: Definition list ends without a blank line; unexpected unindent.
    # Look for a non-newline, followed by a newline, followed by 0-2 spaces, followed by ``
    rst_data = re.sub(r'([.]{1})\n[ ]{0,2}``', r'\1\n\n``', rst_data)
    
    # Clean up instances of the following error:
    # WARNING: Line block ends without a blank line.
    # Not worth it. Not many occurrences and the regex isugly.

    # Clean up instances of the following error:
    # WARNING: Block quote ends without a blank line; unexpected unindent.


    #ticket_matches = re.findall(r"`\(Ticket#([\d]*)\). <https://condor-wiki.cs.wisc.edu/index.cgi/tktview?tn=([\d]*)>`__", rst_data)
    #index_matches = re.findall(r"`[\s]*<index://([\w\s\.\(\)\-,<>%]*)[;]?([\w\s%]*)>[\s]*`[_]*", rst_data)
    #for match in ticket_matches:
        #print(str(match))
    #all_matches = re.findall(r'<div class="verbatim" id="[\w\s-]*">([\w\s\-\.\(\)\'\\="<>!@#$%^&*:;,/|]*)</div>', html_data)
    #print("Number of matches: " + str(len(ticket_matches)))

    # Write the output
    output_file = open(output_filename, "w")
    output_file.write(rst_data)
    output_file.close()


if __name__ == "__main__":
    main()
