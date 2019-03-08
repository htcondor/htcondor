#!/usr/bin/env python

import bs4
import os
import re
import sys

from url_data import *



def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))


# Cleans up the <br/> and newline formatting inside a list of markup elements
def cleanup_pre_contents(data):
    # First, rip out all the newlines
    for index, token in enumerate(data):
        if token:
            utf8_token = token.encode("utf-8")
            if utf8_token == "\n":
                data.remove(token)
    # Next, add a newline bs4.element.NavigableString after every <br/> tag
    for index, token in enumerate(data):
        if token:
            utf8_token = token.encode("utf-8")
            if "<br/>" in utf8_token:
                newline_tag = bs4.element.NavigableString("\n")
                data.insert(index + 1, newline_tag)
    return data


# Cleans up the markup of a table
def cleanup_table(data):
    # First, determine how many columns this table has.
    num_columns = 0
    for index, token in enumerate(data):
        if token.name == "tr":
            this_row_columns = len(list(token.children))
            if this_row_columns > num_columns:
                num_columns = this_row_columns
    # Now determine which elements we want to remove from the table.
    # Do not remove them now, that causes the iterator to miss elements.
    # Instead flag them for removal later.
    tokens_to_remove = []
    for index, token in enumerate(data):
        if type(token) is bs4.element.Tag:
            # Flag all the <tr class="hline">...</tr> tags
            if token.name == "tr" and "class" in token.attrs.keys():
                tokens_to_remove.append(token)
                #print("Marking index " + str(index) + " for removal")
            # Flag all the <colgroup> tags
            elif token.name == "colgroup":
                tokens_to_remove.append(token)
                #print("Marking index " + str(index) + " for removal")
            # Flag all rows with an incorrect number of columns.
            # Looks like this is what's causing pandoc to trip up.
            elif len(list(token.children)) != num_columns:
                tokens_to_remove.append(token)
                #print("Marking index " + str(index) + " for removal")
    # Now, finally, remove all the bad elements.
    for index, token in enumerate(tokens_to_remove):
        data.remove(token)
    return data

# Updates a link URL and also adds in the correct title
def update_link(data):
    updated_link = data
    # Try updating the link. If the link is external, the following block will fail and the link will remain as-is.
    try:
        # Determine what page this is linking to
        old_url = data['href'].split("#")[0]
        updated_link['href'] = updated_urls[old_url]
        updated_link.string = updated_titles[old_url]
    except:
        print("Link " + str(data) + " is external, ignoring")
    return updated_link

# Generate an HTML link we'll use later to import the index.
# We have no way of converting these directly to Sphinx index roles.
# By generating a link instead, we can use a regex to convert them to roles later.
def create_index_entry(index_tag):
    # Open the old index markup, parse it into a soup object
    with open("old-index.html", "r") as old_index_file:
        old_index_data = old_index_file.read()
    old_index_file.close()
    index_soup = bs4.BeautifulSoup(old_index_data, features="lxml")

    # Now we need to extract the index name (+ possibly parent name) from this markup.
    # Scan the old markup for the id value of the tag that got passed in.
    item_name = ""
    parent_name = ""
    a_tags = index_soup.find_all("a")
    #print("Looking for id = " + str(index_tag['id']))
    for a_tag in a_tags:
        if "href" in a_tag.attrs.keys():
            if index_tag['id'] in a_tag['href']:
                #print("\nid = " + index_tag['id'] + ", a_tag.parent = " + repr(a_tag.parent))
                #print("\na_tag.parent.contents = " + repr(a_tag.parent.contents))
                #print("type(a_tag.parent.contents[0]) = " + str(type(a_tag.parent.contents[0])))
                if type(a_tag.parent.contents[0]) is bs4.element.NavigableString:
                    item_name = a_tag.parent.contents[0].strip().encode('ascii', 'ignore').decode('ascii')
                elif type(a_tag.parent.contents[0]) is bs4.element.Tag:
                    item_name = a_tag.parent.contents[0].contents[0].strip().encode('ascii', 'ignore').decode('ascii')
                # If this record is a subitem, we want to find the name of the parent item.
                if "class" in a_tag.parent.attrs.keys():
                    if a_tag.parent['class'][0] == "index-subitem":
                        # Iterate backwards through siblings until we find it.
                        for previous_sibling in a_tag.parent.previous_siblings:
                            if type(previous_sibling) == bs4.element.Tag:
                                if "class" in previous_sibling.attrs.keys():
                                    if previous_sibling['class'][0] == "index-item":
                                        parent_name = previous_sibling.contents[0].strip().encode('ascii', 'ignore').decode('ascii')
                                        break
                break

    # Finish cleaning up the index name + parent name, and set them as the link href
    if item_name:
        if item_name[-1:] == ",":
            item_name = item_name[:-1]
        if parent_name:
            if parent_name[-1:] == ",":
                parent_name = parent_name[:-1]
            index_tag['href'] = "index://" + item_name + ";" + parent_name
        else:
            index_tag['href'] = "index://" + item_name
    print("id = " + str(index_tag['id']) + ", index_tag = " + str(index_tag))

    return index_tag

def main():

    # Command line arguments
    if len(sys.argv) < 2:
        print("Usage: html-cleanup.py <input-file> [Optional]: <output-file>")
        sys.exit(1)
    
    # Open the input file
    input_filename = str(sys.argv[1])
    with open(input_filename, "r") as input_file:
        html_data = input_file.read()
    input_file.close()

    # Check if output file was specified, if so record it
    output_filename = input_filename + ".out"
    if len(sys.argv) >= 3:
        output_filename = str(sys.argv[2])

    # Now start The Great HTML Parse!
    print("Cleaning up " + input_filename + " to " + output_filename)
    soup = bs4.BeautifulSoup(html_data, features="lxml")

    # STEP 1
    # Delete things we don't want. Do this before editing any markup.

    # Delete the navigation tags (defined by <span style="font-size: 200%;>...</span>")
    nav_tags = soup.find_all("span", attrs={"style": "font-size: 200%;"})
    #print("Found " + str(len(nav_tags)) + " navigation spans")
    for tag in nav_tags:
        tag.decompose()

    # Delete the "Contents" and "Index" links. Also delete all empty links.
    # Wait, we can use the empty links to identify indexes. Keep them for now.
    links_tags = soup.find_all("a")
    for tag in links_tags:
        if tag.string:
            if tag.string == "Contents" or tag.string == "Index":
                tag.decompose()
        #else:
        #    # We don't want to delete links that have an href set
        #    if "href" not in tag.attrs:
        #        tag.decompose()

    # Delete the table of contents
    toc_tags = soup.find_all("div", attrs={"class": "sectionTOCS"})
    for tag in toc_tags:
        tag.decompose()

    # Delete the section numbering
    titlemark_tags = soup.find_all("span", attrs={"class": "titlemark"})
    for tag in titlemark_tags:
        tag.decompose()

    # Delete all hidden spans
    hidden_span_tags = soup.find_all("span", attrs={"style": "visibility: hidden;"})
    print("Found " + str(len(hidden_span_tags)) + " hidden spans")
    for tag in hidden_span_tags:
        tag.decompose()

    # At this point, soup is sufficiently confused that we should reload the contents of our HTML markup
    html_dump = str(soup)
    soup = bs4.BeautifulSoup(html_dump, features="lxml")

    # STEP 2
    # Now manipulate the elements we want to keep

    # Convert all <span class="textbf">...</span> tags to <b>...</b>
    bold_tags = soup.find_all("span", attrs={"class": "textbf"})
    print("Found " + str(len(bold_tags)) + " textbf spans")
    for tag in bold_tags:
        new_tag = soup.new_tag("b")
        if tag.string:
            new_tag.string = tag.string
        tag.replace_with(new_tag)

    # Convert all <span class="textit">...</span> tags to <i>...</i>
    italic_tags = soup.find_all("span", attrs={"class": "textit"})
    print("Found " + str(len(italic_tags)) + " textit spans")
    for tag in italic_tags:
        new_tag = soup.new_tag("i")
        if tag.string:
            new_tag.string = tag.string
        #print("new_tag = " + str(new_tag))
        #print("new_tag.parent" + str(new_tag.parent))
        tag.replace_with(new_tag)

    # Convert all <br class="newline" /> tags to <br />
    br_newline_tags = soup.find_all("br", attrs={"class": "newline"})
    print("Found " + str(len(br_newline_tags)) + " br newline tags")
    for tag in br_newline_tags:
        new_tag = soup.new_tag("br")
        tag.replace_with(new_tag)

    # Convert all <h3> tags to <h1>
    h3_tags = soup.find_all("h3")
    print("Found " + str(len(h3_tags)) + " h3 tags")
    for tag in h3_tags:
        #print("tag = " + str(tag))
        new_tag = soup.new_tag("h1")
        if tag.string:
            #print("tag.string = " + tag.string)
            new_tag.string = tag.string
        else:
            if tag.contents:
                #print("tag.contents = " + str(tag.contents))
                new_tag.contents = tag.contents
        tag.replace_with(new_tag)

    # Soup is confused. Reload.
    html_dump = str(soup)
    soup = bs4.BeautifulSoup(html_dump, features="lxml")

    # Convert all <h4> tags to <h2>
    h4_tags = soup.find_all("h4")
    print("Found " + str(len(h4_tags)) + " h4 tags")
    for tag in h4_tags:
        new_tag = soup.new_tag("h2")
        if tag.string:
            new_tag.string = tag.string
        else:
            if tag.contents:
                new_tag.contents = tag.contents
        tag.replace_with(new_tag)

    # Convert all <h5> tags to <h3>
    h5_tags = soup.find_all("h5")
    print("Found " + str(len(h5_tags)) + " h5 tags")
    for tag in h5_tags:
        new_tag = soup.new_tag("h3")
        if tag.string:
            new_tag.string = tag.string
        else:
            if tag.contents:
                new_tag.contents = tag.contents
        tag.replace_with(new_tag)

    # Soup is confused. Reload.
    html_dump = str(soup)
    soup = bs4.BeautifulSoup(html_dump, features="lxml")

    # Convert all <div class="verbatim">...</div> tags to <pre>...</pre>
    pre_tags = soup.find_all("div", attrs={"class": "verbatim"})
    print("Found " + str(len(pre_tags)) + " verbatim divs")
    for tag in pre_tags:
        new_tag = soup.new_tag("pre")
        new_tag.contents = cleanup_pre_contents(tag.contents)
        tag.replace_with(new_tag)

    # Soup is confused. Reload.
    html_dump = str(soup)
    soup = bs4.BeautifulSoup(html_dump, features="lxml")

    # Convert all <code>...</code> tags to <pre>...</pre>
    code_tags = soup.find_all("code")
    print("Found " + str(len(code_tags)) + " code divs")
    for tag in code_tags:
        new_tag = soup.new_tag("pre")
        new_tag.contents = cleanup_pre_contents(tag.contents)
        tag.replace_with(new_tag)

    # Convert all <span class="texttt">...</span> tags to <code>...</code>.
    # This needs to happen after converting all <code> tags to <pre>
    spantt_tags = soup.find_all("span", attrs={"class": "texttt"})
    print("Found " + str(len(spantt_tags)) + " texttt spans")
    for tag in spantt_tags:
        new_tag = soup.new_tag("code")
        if tag.string:
            new_tag.string = tag.string
        else:
            if tag.contents:
                new_tag.contents = tag.contents
        tag.replace_with(new_tag)

    # Cleanup table formatting so that pandoc knows what to do with them
    table_tags = soup.find_all("table")
    print("Found " + str(len(table_tags)) + " table tags")
    for tag in table_tags:
        new_tag = soup.new_tag("table")
        new_tag.contents = cleanup_table(tag.contents)
        tag.replace_with(new_tag)

    # Update links to point to correct URLs
    a_tags = soup.find_all("a")
    print("Found " + str(len(a_tags)) + " a tags")
    for tag in a_tags:
        if "href" in tag.attrs.keys():
            updated_link = update_link(tag)
            tag.replace_with(updated_link)

    # Soup is confused. Reload.
    html_dump = str(soup)
    soup = bs4.BeautifulSoup(html_dump, features="lxml")

    # Update images to point to correct src files
    # .... not worth it. We only have ~10 images in strange formats. Do it manually.

    # Is there anything we can do to automate the index items?
    # The following code generates <a> tags that contain the index text
    # That should be enough for us to write an RST-scrubber to generate the actual directives.
    index_tags = soup.find_all("a")
    print("Found " + str(len(index_tags)) + " a tags which may or may not be index items")
    for tag in index_tags:
        if "id" in tag.attrs.keys():
            new_tag = create_index_entry(tag)
            tag.replace_with(new_tag)


    # Does outputting pretty HTML matter? 
    # Actually we do NOT want pretty HTML, since this introduces spacing that confuses pandoc.
    #pretty_html = soup.prettify().encode('utf-8')
    #print(str(pretty_html))
    output_html = str(soup)

    # Overwrite the original file with the parseable markup
    output_file = open(output_filename, "w")
    output_file.write(output_html)
    output_file.close()

if __name__ == "__main__":
    main()
