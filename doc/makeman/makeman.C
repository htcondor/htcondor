/**************************************************************************
 *
 * makeman.C
 * by Alain Roy
 * December 19, 2001
 * 
 * makeman is a program for generating nroff man pages from our HTML 
 * manual pages. It works pretty well, but there are some things to note
 * about it. 
 * 
 * 1) This is not a generic HTML to nroff converter. It does a decent job
 *    with the HTML that we get from latex2html for our current man pages, 
 *    but it certainly does not handle all HTML. One big example is that
 *    tables are not handled. Images are not handled, and probably can 
 *    never be. 
 * 
 * 2) This was a quick weekend hack. I started it without knowing a whole 
 *    lot about what problems I would encounter on the way. The resulting
 *    is is not particularly beautiful. It was written on my own time, 
 *    since it wasn't on anyone's priority list for Condor. It will be 
 *    improved on my time. That is, it probably won't be improved much. 
 *
 * 3) Because it's a quick weekend hack, it is more fragile than you might
 *    expect. Handling a new HTML tag may disrupt how other tags are handled,
 *    unless you are fairly familiar with how it all works. 
 *
 * 4) It's a good idea not to use groff-specific stuff, but limit yourself
 *    to nroff. 
 *
 * 5) Here are some web pages I found useful in developing this program. 
 *
 * http://sunsite.ualberta.ca/Documentation/Gnu/groff-1.16/html_node/groff_toc.html
 * http://unix.about.com/cs/nroffandtroff/
 * http://www-oss.fnal.gov/~mengel/man_page_notes.html
 *
 **************************************************************************/
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>

using namespace std;

// A very simple class for parsing the command-line parameters
class Parameters
{
public:
	bool   debug;
	bool   verbose;
	string input_file;
	string output_file;
	string section_number;

	void parse_parameters(int argc, char **argv);
};

enum TokenType
{
	tokenType_EOF,
	tokenType_Text,
	tokenType_Tag
};

// The list of HTML tags that we recognize. 
// Note that this is much less than the entire list of tags.
// Also note that if we don't recognize a tag while reading a file, 
//   a message is printed to stdout. 
// Finally, note that just because we recognize a tag doesn't mean
//   that we actually do anything with it. 
enum TagType
{
	tagType_NotATag,        // Therefore, text.
	tagType_Anchor,         // <a>
	tagType_Paragraph,      // <p>
	tagType_BreakLine,      // <br>
	tagType_Image,          // <img>
    tagType_Bold,           // <b>
    tagType_Strong,         // <strong>
	tagType_Underline,      // <u>
	tagType_Doctype,        // <!DOCTYPE>
	tagType_HTML,           // <html>
	tagType_Head,           // <head>
	tagType_Meta,           // <meta>
	tagType_Link,           // <link>
	tagType_Title,          // <title>
	tagType_Body,           // <body>
	tagType_H1,             // <h1>
	tagType_H2,             // <h2>
	tagType_H3,             // <h3>
	tagType_Italic,         // <i>
	tagType_Emphasized,     // <em>
	tagType_OrderedList,    // <ol>
	tagType_UnorderedList,  // <ul>
	tagType_DefinitionList, // <dl>
	tagType_ListElem,       // <li>
	tagType_DefinitionTerm, // <dt>
	tagType_Definition,     // <dd>
	tagType_Preformatted,   // <pre>
	tagType_Code,           // <code>
	tagType_Typewriter,     // <tt>
	tagType_Rule,           // <hr>
	tagType_Address,        // <address>
	tagType_Comment,        // <!-- some text -->
	tagType_Unknown         // Anything we don't yet parse.
};

// Some constants for dealing with lists. 
enum
{
	MAX_LIST_DEPTH  = 6,
	UNORDERED_LIST  = -1,
	DEFINITION_LIST = -2,
	COMPACT_DEFINITION_LIST
};

// Our representation of a token. 
// Given the simplicity of the class, you may ask why this isn't 
// just a struct, or why I didn't add methods to it. Recall, this
// was a quick weekend hack. 
class Token
{
public:
	TokenType  type;
	TagType    tag_type;
	bool       tag_begin; // true, for example if <p>, false if </p>
	string     text;
	bool       compact;
};

// We find stuff that is bracketed by comments, and we want to ignore it. 
// For example, we have:
// <!--Navigation Panel-->
// <!--End of Navigation Panel-->
// And we want to ignore everything in between. That is what these 
// comment_markers are for
#define NUMBER_OF_COMMENT_MARKERS (sizeof(comment_markers) / sizeof(char *))
const char *comment_markers[] = 
{
	"Navigation Panel",
	"Child-Links"
};

// Function prototypes
static void process_file(ifstream &input_file, ofstream &output_file, const Parameters &parameters);
static string html_to_nroff(const string html_text, const Parameters &parameters);
static Token *extract_token(int *token_start, const string &line, bool skip_whitespace);
static void discover_tag_type(Token *token);
static void add_fixed_characters(string &source, string &dest, bool in_pre_text);

/**************************************************************************
 *
 * Function: main
 *
 **************************************************************************/
int main(int argc, char **argv)
{
	Parameters parameters;

	parameters.parse_parameters(argc, argv);

	ifstream input_file(parameters.input_file.c_str());
	ofstream output_file(parameters.output_file.c_str());
	if (!input_file) {
		cout << "Can't open file: \"" << parameters.input_file
			 << "\"\n";
	} else if (!output_file) {
		cout << "Can't open file: \"" << parameters.input_file
			 << "\"\n";
	} else {
		if (parameters.verbose) {
			printf("Converting %s to %s\n", 
				   parameters.input_file.c_str(), parameters.output_file.c_str());
		}
#if (__GNUC__<3) 
		input_file.flags(0);
#else
		input_file.flags((std::ios_base::fmtflags)0);
#endif
		process_file(input_file, output_file, parameters);
	}

	return 0;
}

/**************************************************************************
 *
 * Function: process_file
 * Purpose:  Read in the input file, convert it to nroff, stuff it into
 *           the output file.
 *
 **************************************************************************/
static void process_file(
	ifstream &input_file, 
	ofstream &output_file, 
	const Parameters &parameters)
{
	int    line_number;
	string line;
	string html_text;
	string nroff_text;

	line_number = 0;
	while (1) {
		getline(input_file, line, '\n');
		line += '\n';
		if (input_file.eof()) {
			break;
		} else {
			line_number++;
			html_text += line;
		}
	}
	nroff_text = html_to_nroff(html_text, parameters);
	output_file << nroff_text;
	return;
}

/**************************************************************************
 *
 * Function: html_to_nroff
 * Purpose:  Convert a bunch of html (in a string) into nroff. 
 * Returns:  A string containing the nroff.
 *
 **************************************************************************/
static string html_to_nroff(
	const string html_text, 
	const Parameters &parameters)
{
	int    token_start, length, i;
	string *nroff_text;
	string command_name;
	Token  *token;

	int lists_status[MAX_LIST_DEPTH];
	int current_list = -1;

	time_t  seconds;

	nroff_text = new string;

	// Find the name of the command, presuming it is the name 
	// of the HTML file, up to the period. For example, if the input
	// file is condor_submit.html, we'll decide the command name is condor_submit. 
	// (Is this a good idea, or should we allow a parameter to specify it?)
	// We then use the command_name within nroff.
	length = parameters.input_file.size();
	for (i = 0; i < length; i++) {
		if (parameters.input_file[i] == '.') {
			break;
		} else {
			command_name += parameters.input_file[i];
		}
	}

	// Begin with a comment header.
	*nroff_text += ".\\\"Man page for ";
	*nroff_text += command_name;
	*nroff_text += "\n";
	*nroff_text += ".\\\"Generated by makeman on ";
	time(&seconds);
	*nroff_text += ctime(&seconds); // Note that ctime will give us a newline.

	// Next the .TH command, to set up the man page.
	*nroff_text += ".TH ";
	*nroff_text += command_name;
	*nroff_text += " 1 "; // The section number
	*nroff_text += "date\n"; // To be filled in.

	token_start = 0;

	// Here is the big loop that considers each token in the HTML
	// and converts it to nroff. 
	bool ignore_region = false;
	bool in_title = false;
	bool in_paragraph = false;
	bool in_pre_text  = false;
	bool just_finished_pre_text = false;
	bool last_output_did_newline = true;
	while (1) {
		char num_string[10];

		token = extract_token(&token_start, html_text, !in_pre_text);
		if (token->type == tokenType_EOF) {
			break;
		} else if (ignore_region && token->tag_type != tagType_Comment) {
			// Recall that we ignore regions of HTML based on 
			// the begin/end comments that Latex2HTML thoughtfully provides us.
			continue;
		} else if (in_title && token->tag_type != tagType_Title) {
			// We also ignore anything within <title></title>
			continue;
		} else if (token->type == tokenType_Text) {
			// A "token" of text isn't just a single word, but
			// it is a whole block of text. We spit it out in 
			// a couple of different possible ways. 
			if (just_finished_pre_text == true) {
				if (current_list >= 0) {
					*nroff_text += ".IP \"\" ";
					sprintf(num_string, "%d\n", (current_list+1) * 3);
					*nroff_text += num_string;
				} else {
					*nroff_text += ".P\n";
				}
				just_finished_pre_text = false;
				last_output_did_newline = true;
			}
			// We might end up spitting out a period at the beginning of a line.
			// This will be interpreted as a command to nroff, so we escape it. 
			// Note that we end up with a problem. If someone has something like:
			// "<b>blah</b>.", we will output the period on a line by itself, and 
			// nroff will put a space after blah, before the period. I'm not 
			// sure how to get around that. 
			if (last_output_did_newline && token->text[0] == '.') {
				*nroff_text += "\\&";
			}
			add_fixed_characters(token->text, *nroff_text, in_pre_text);
			last_output_did_newline = in_pre_text;
		} else {
			assert(token->type == tokenType_Tag);
			switch (token->tag_type) {
			case tagType_H1:
				break;
			case tagType_H2:
				if (token->tag_begin) {
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					*nroff_text += ".SH ";
					last_output_did_newline = false;
				} else {
					*nroff_text += "\n";
					last_output_did_newline = true;
				}
				break;
			case tagType_H3:
				if (token->tag_begin) {
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					*nroff_text += ".SS ";
					last_output_did_newline = false;
				} else {
					*nroff_text += "\n";
					last_output_did_newline = true;
				}
				break;
			case tagType_Anchor:
				break;
			case tagType_Paragraph:
				if (token->tag_begin) {
					in_paragraph = true;
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					if (current_list >= 0) {
						*nroff_text += ".IP \"\" ";
						sprintf(num_string, "%d\n", (current_list+1) * 3);
						*nroff_text += num_string;
					} else {
						*nroff_text += ".P\n";
					}
					last_output_did_newline = true;
				} else {
					in_paragraph = false;
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					last_output_did_newline = true;
				}
				break;
			case tagType_Preformatted:
				in_pre_text = token->tag_begin;
				if (!last_output_did_newline) {
					*nroff_text += "\n";
				}
				if (token->tag_begin) {
					*nroff_text += ".P\n";
				} else {
					just_finished_pre_text = true;
				}
				break;
			case tagType_Italic:
			case tagType_Underline:
			case tagType_Emphasized:
				if (token->tag_begin) {
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					last_output_did_newline = false;
					*nroff_text += ".I ";
				} else {
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					last_output_did_newline = true;
				}
				break;
			case tagType_Bold:
			case tagType_Strong:
				if (token->tag_begin) {
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					last_output_did_newline = false;
					*nroff_text += ".B ";
				} else {
					if (!last_output_did_newline) {
						*nroff_text += "\n";
					}
					last_output_did_newline = true;
				}
				break;
			case tagType_OrderedList:
				if (token->tag_begin) {
					current_list++;
					if (current_list < MAX_LIST_DEPTH) {
						lists_status[current_list] = 0;
					}
				} else {
					if (current_list >= 0) {
						current_list--;
					}
				}
				break;
			case tagType_UnorderedList:
				if (token->tag_begin) {
					current_list++;
					if (current_list < MAX_LIST_DEPTH) {
						lists_status[current_list] = UNORDERED_LIST;
					}
				} else {
					if (current_list >= 0) {
						current_list--;
					}
				}
				break;
			case tagType_DefinitionList:
				if (token->tag_begin) {
					current_list++;
					if (current_list < MAX_LIST_DEPTH) {
						if (token->compact) {
							lists_status[current_list] = COMPACT_DEFINITION_LIST;
						} else {
							lists_status[current_list] = DEFINITION_LIST;
						}
					}
				} else {
					if (current_list >= 0) {
						current_list--;
					}
				}
				break;
			case tagType_ListElem:
				if (token->tag_begin && current_list < MAX_LIST_DEPTH) {
  					if (!last_output_did_newline) {
  						*nroff_text += "\n";
  					}
					
					*nroff_text += ".IP \"\" ";
					sprintf(num_string, "%d\n", (current_list+1) * 3);
					*nroff_text += num_string;
					
  					if (lists_status[current_list] == UNORDERED_LIST) {
  						*nroff_text += "* ";
  					} else {
  						lists_status[current_list]++;
  						sprintf(num_string, "%d. ", 
								lists_status[current_list]);
  						*nroff_text += num_string;
  					}
  					last_output_did_newline = false;
				}
				break;
			case tagType_DefinitionTerm:
				if (current_list < MAX_LIST_DEPTH) {
  					if (!last_output_did_newline) {
  						*nroff_text += "\n";
  					}
					if (token->tag_begin) {
						if (current_list > 0) {
							*nroff_text += ".IP \"\" ";
							if (lists_status[current_list] == DEFINITION_LIST) {
								sprintf(num_string, "%d\n", current_list * 3);
								last_output_did_newline = true;
							} else {
								// A compact list
								sprintf(num_string, "%d\n", (current_list+1) * 3);
								last_output_did_newline = true;
							}
							*nroff_text += num_string;
						} else {
							*nroff_text += ".P\n";
						}
					}
				}
				break;
			case tagType_Definition:
				if (current_list < MAX_LIST_DEPTH
					&& lists_status[current_list] != COMPACT_DEFINITION_LIST) {
  					if (!last_output_did_newline) {
  						*nroff_text += "\n";
  					}
  					*nroff_text += ".IP \"\" ";
  					sprintf(num_string, "%d\n", (current_list+1) * 3);
  					*nroff_text += num_string;

  					last_output_did_newline = true;
				}
				break;
			case tagType_Comment:
				for (int i = 0; i < NUMBER_OF_COMMENT_MARKERS; i++) {
					if (token->text.find(comment_markers[i]) != -1) {
						ignore_region = !ignore_region;
					}
				}
				break;
			case tagType_Title:
				if (token->tag_begin) {
					*nroff_text += ".SH Name\n.P\n";
					in_title = true;
  					last_output_did_newline = true;
				} else {
					in_title = false;
					// We hold over the newline from the begin tag,
					// since nothing was output for the title.
  					last_output_did_newline = true;
				}
				break;
			case tagType_BreakLine:
			case tagType_Image:
				break;
			case tagType_Code:
			case tagType_Typewriter:
				// incorrectly injects space character before
				// punctuation marks, but without, words are
				// mashed together
				*nroff_text += " ";
				 break;
			case tagType_Head:
			case tagType_Meta:
			case tagType_Link:
			case tagType_Doctype:
			case tagType_HTML:
			case tagType_Body:
			case tagType_Rule:
			case tagType_Address:
				// We ignore these tags.
				break;
			case tagType_Unknown:
				cout << "Ignoring unknown tag: " << token->text << endl;
				break;
			case tagType_NotATag:
				// Ignore things that aren't tags.
				break;
			} // end of switch on tag type
		}
		if (token->tag_type != tagType_Preformatted) {
			just_finished_pre_text = false;
		}
		delete token;
	}

	*nroff_text += "\n";

	return *nroff_text;
}

/**************************************************************************
 *
 * Function: extract_token
 * Purpose:  Pull out the next token from our html text. 
 * Returns:  A token, freshly allocated. It's up to the caller to delete it.
 *
 **************************************************************************/
static Token *extract_token(int *token_start, const string &line, bool skip_whitespace)
{
	int token_end;
	unsigned int line_size;
	bool in_tag, in_quote;
	Token  *token;

	token = new Token;
	line_size = line.size();
	if ((unsigned int) *token_start >= line_size) {
		token->type = tokenType_EOF;
		token->text = "";
	}
	else {
		// First, skip whitespace
		if (skip_whitespace) {
			while (isspace(line[*token_start])) {
				(*token_start)++;
			}
		}
		if (line[*token_start] == '<') {
			in_tag = true;
			token->type = tokenType_Tag;
		} else {
			in_tag = false;
			token->type = tokenType_Text;
		}
		token_end = *token_start;

		if (in_tag) {
			// Scan until the end of the tag. 
			while (token_end < line_size && line[token_end] != '>') {
				token_end++;
			}
		} else {
			// Scan until we reach a tag. 
			while (token_end < line_size && line[token_end] != '<') {
				token_end++;
			}
			if (line[token_end] == '<') {
				token_end--;
			}
		}
		if (*token_start == token_end + 1) { // Read no charaters
			token->text = "";
			token->type = tokenType_EOF;
			token->tag_type = tagType_NotATag;
		}
		else {
			token->text = line.substr(*token_start, token_end-(*token_start) + 1);
			*token_start = token_end + 1;
			if (token->type == tokenType_Text) {
				token->tag_type = tagType_NotATag;
			} else {
				discover_tag_type(token);
				if (token->tag_type == tagType_Image){
					// Check for some special cases we know about that
					// latex2html does.
					if (token->text.find("ALT=\"$&lt;$\"") != -1) {
						token->type     = tokenType_Text;
						token->tag_type = tagType_NotATag;
						token->text     = "<";
					} else if (token->text.find("ALT=\"$&gt;$\"") != -1) {
						token->type     = tokenType_Text;
						token->tag_type = tagType_NotATag;
						token->text     = ">";
					} else if (token->text.find("ALT=\"$&lt;=$\"") != -1) {
						token->type     = tokenType_Text;
						token->tag_type = tagType_NotATag;
						token->text     = ">=";
					} else if (token->text.find("ALT=\"$&gt;=$\"") != -1) {
						token->type     = tokenType_Text;
						token->tag_type = tagType_NotATag;
						token->text     = ">=";
					}
				}
			}
		}
	}
	return token;
}

/**************************************************************************
 *
 * Function: discover_tag_type
 * Purpose:  Given a token that begins with a less than symbol, we know
 *           that we have an HTML tag. This is essentially a big case
 *           to extract the type of the tag. For instance, if we have
 *           <DL compact>, the type is tagType_DefinitionList. 
 *
 **************************************************************************/
static void discover_tag_type(Token *token)
{
	int      i, len;
	string   tag_name;
	TagType  type;

	assert(token->text[0] == '<');

	token->compact = false;
	// We have to check comments separately, because there isn't 
	// necessarily a whitespace after the two hyphens, and the 
	// rest of the tag checking assumes that there is whitespace
	// after the tag name.
	if (!strncmp(token->text.c_str(), "<!--", 4)){
		token->tag_type = tagType_Comment;
	}
	else {
		len = token->text.size();
		for (i = 1; i < len; i++) {
			if (isspace(token->text[i]) || token->text[i] == '>') {
				break;
			} else { 
				tag_name += tolower(token->text[i]);
			}
		}
		token->tag_type = tagType_NotATag;
		if (tag_name[0] == '/') {
			token->tag_begin = false;
			tag_name = tag_name.substr(1);
		} else {
			token->tag_begin = true;
		}
		if (!tag_name.compare("a")) {
			token->tag_type = tagType_Anchor;
		} else if (!tag_name.compare("p")) {
			token->tag_type = tagType_Paragraph;
		} else if (!tag_name.compare("br")) {
			token->tag_type = tagType_BreakLine;
		} else if (!tag_name.compare("img")) {
			token->tag_type = tagType_Image;
		} else if (!tag_name.compare("b")) {
			token->tag_type = tagType_Bold;
		} else if (!tag_name.compare("strong")) {
			token->tag_type = tagType_Strong;
		} else if (!tag_name.compare("u")) {
			token->tag_type = tagType_Underline;
		} else if (!tag_name.compare("!doctype")) {
			token->tag_type = tagType_Doctype;
		} else if (!tag_name.compare("html")) {
			token->tag_type = tagType_HTML;
		} else if (!tag_name.compare("head")) {
			token->tag_type = tagType_Head;
		} else if (!tag_name.compare("meta")) {
			token->tag_type = tagType_Meta;
		} else if (!tag_name.compare("link")) {
			token->tag_type = tagType_Link;
		} else if (!tag_name.compare("title")) {
			token->tag_type = tagType_Title;
		} else if (!tag_name.compare("body")) {
			token->tag_type = tagType_Body;
		} else if (!tag_name.compare("h1")) {
			token->tag_type = tagType_H1;
		} else if (!tag_name.compare("h2")) {
			token->tag_type = tagType_H2;
		} else if (!tag_name.compare("h3")) {
			token->tag_type = tagType_H3;
		} else if (!tag_name.compare("i")) {
			token->tag_type = tagType_Italic;
		} else if (!tag_name.compare("em")) {
			token->tag_type = tagType_Emphasized;
		} else if (!tag_name.compare("ol")) {
			token->tag_type = tagType_OrderedList;
		} else if (!tag_name.compare("ul")) {
			token->tag_type = tagType_UnorderedList;
		} else if (!tag_name.compare("dl")) {
			if (token->text.find("compact") != -1 
				|| token->text.find("COMPACT") != -1) {
				token->compact = true;
			} else {
				token->compact = false;
			}
			token->tag_type = tagType_DefinitionList;
		} else if (!tag_name.compare("li")) {
			token->tag_type = tagType_ListElem;
		} else if (!tag_name.compare("dt")) {
			token->tag_type = tagType_DefinitionTerm;
		} else if (!tag_name.compare("dd")) {
			token->tag_type = tagType_Definition;
		} else if (!tag_name.compare("pre")) {
			token->tag_type = tagType_Preformatted;
		} else if (!tag_name.compare("code")) {
			token->tag_type = tagType_Code;
		} else if (!tag_name.compare("tt")) {
			token->tag_type = tagType_Typewriter;
		} else if (!tag_name.compare("hr")) {
			token->tag_type = tagType_Rule;
		} else if (!tag_name.compare("address")) {
			token->tag_type = tagType_Address;
		} else {
			token->tag_type = tagType_Unknown;
		}
	}
	return;
}

/**************************************************************************
 *
 * Function: add_fixed_characters
 * Purpose:  In HTML text, we might have special characters like &lt;. 
 *           We convert those. (Note that we skip a lot of special characters
 *           though, so we might need to add more eventually.) We also 
 *           play with spaces, tabs, and newlines. Normally we remove them, 
 *           unless we are in preformatted text (<pre>). 
 *
 **************************************************************************/
static void add_fixed_characters(string &source, string &dest, bool in_pre_text)
{
	int source_length;
	bool in_spaces = false;

	source_length = source.length();
	for (int offset = 0; offset < source_length; offset++) {
		if (source[offset] == '\n') {
			if (in_pre_text) {
				if (offset == (source_length - 1)) {
					dest += '\n';
				} else {
					dest += "\n.br\n";
				}
				in_spaces = false;
			} else {
				if (!in_spaces) {
					dest += " ";
				}
				in_spaces = true;
			}
		} else if (source[offset] == '&') {
			// We may have to do an entity translation here.
			if (offset == (source_length - 1) || isspace(source[offset+1])) {
				// No translation, it's a plain ampersand
				dest += source[offset];
			} else {
				// Make a string of the characters following the 
				// ampersand, up to the semicolon
				string entity;
				int old_offset;
				old_offset = offset;
				offset++;
				while (offset < source_length) {
					if (source[offset] == ';' || source[offset] == '\n') {
						break;
					} else {
						entity += source[offset];
						offset++;
					}
				}
				if (!entity.compare("lt")) {
					dest += '<';
				} else if (!entity.compare("gt")) {
					dest += '>';
				} else if (!entity.compare("amp")) {
					dest += '&';
				} else if (!entity.compare("nbsp")) {
					dest += ' ';
				} else if (!entity.compare("#160")) {
					dest += ' ';
				} else if (!entity.compare("#169")) {
					dest += "(C)";
				} else {
					// We can't translate it. 
					offset = old_offset;
					dest += '&';
				}
			}
			in_spaces = false;
		} else if (source[offset] == '\\') {
			dest += "\\\\";  // replace backslash with double backslash.
		} else if ((!in_pre_text && source[offset] == ' ') || source[offset] == '\t') {
			if (!in_spaces) {
				dest += ' '; // tabs become spaces.
			} 
			in_spaces = true;
		} else {
			in_spaces = false;
			dest += source[offset];
		}
	}
	return;
}

/**************************************************************************
 *
 * Function: parse_parameters
 * Purpose:  Examine the command line parameters.
 *
 **************************************************************************/
void Parameters::parse_parameters(int argc, char **argv)
{
	// First, set up some defaults;
	debug      = false;
	input_file = "";
	output_file = "";

	// Then we parse to see what the user wants. 
	for (int arg_index = 1; arg_index < argc; arg_index++) {
		if (!strcmp(argv[arg_index], "-d")
			|| !strcmp(argv[arg_index], "-debug")) {
			debug = true;
		} else if (!strcmp(argv[arg_index], "-v")
			|| !strcmp(argv[arg_index], "-verbose")) {
			verbose = true;
		} else if (!strcmp(argv[arg_index], "-i")
				   || !strcmp(argv[arg_index], "-input")) {
			arg_index++;
			if (arg_index >= argc) {
				cout << "Error: Missing input file name." << endl;
				exit(1);
			} else {
				input_file = argv[arg_index];
			}
		} else if (!strcmp(argv[arg_index], "-o")
				   || !strcmp(argv[arg_index], "-output")) {
			arg_index++;
			if (arg_index >= argc) {
				cout << "Error: Missing output file name." << endl;
				exit(1);
			} else {
				output_file = argv[arg_index];
			}
		} else if (!strcmp(argv[arg_index], "-s")
				   || !strcmp(argv[arg_index], "-section")) {
			arg_index++;
			if (arg_index >= argc) {
				cout << "Error: Missing section number." << endl;
				exit(1);
			} else {
				section_number = argv[arg_index];
			}
		} else {
			cout << "Error: unknown argument " << argv[arg_index] << ".\n";
		}
	}

	if (!input_file.compare("")) {
		cout << "Error: you didn't specify an input file.\n";
		cout << "Usage: test_classads [-debug] <input-file>\n";
		exit (1);
	} else if (!output_file.compare("")) {
		if (!section_number.compare("") ) {
			cout << "Error: you didn't specify an output file or a section number.\n";
			exit (1);
		} else {
			int html_start;

			output_file = input_file;
			html_start = output_file.find(".html");
			if (html_start >= 0) {
				output_file.replace(html_start+1, 4, section_number);
			}
		}
	}

	return;
}
