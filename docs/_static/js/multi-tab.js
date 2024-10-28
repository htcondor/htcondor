/*
 * Dynamic multi-tab code block examples.
*/

function ExtractTitle(str, num) {
    /* Create default tab text */
    var tab = 'Option ' + (num + 1);
    /* Split class string by spaces */
    str.split(" ").forEach((option) => {
        var specified = true;
        /* Check each class item for 'tab-{Title}' */
        if ( option.search("tab-") == 0 && option.length >= 5) {
            /* Replace hyphens with spaces and make Title Case */
            tab = option.substring(4).replaceAll("-", " ").replace(/\w\S*/g, (txt) => {
                return txt.charAt(0).toUpperCase() + txt.substr(1).toLowerCase();
            });
            return
        }
    });
    return tab
}

$(function() {
    /* Setup code block option tabs */
    $('div.example-code').each(function() {
        var example_sel = $('<ul />', { class: "example-selector" });
        var i = 0;
        /* Create list tab option for each code block in this multi-tab container */
        $('div[class*="highlight-"]', this).each(function() {
            /* Create tab with same class and specified tab text or 'Option N' */
            var sel_item = $('<li />', {
                class: $(this).attr('class'),
                text: ExtractTitle($(this).attr('class'), i)
            });
            /* Uniquely identify each associated code block incase multiple
               use same lexer with no unique tab name */
            sel_item.addClass('option-' + i)
            $(this).addClass('option-' + i)
            /* Check for associated caption */
            var obj = document.getElementsByClassName($(this).attr('class'))[0];
            var parentElement = obj.parentElement;
            var caption = parentElement.querySelector('.code-block-caption');
            if (caption) {
                $(this).attr('class').split(" ").forEach((cls) => {
                    caption.classList.add(cls);
                });
                caption.classList.add('option-' + i);
                caption.classList.add('example');
            }
            /* Select first in list for display and hide all others */
            if (i++) {
                $(this).hide();
                if (caption) {
                    caption.style.display = 'none';
                }
            } else {
                sel_item.addClass('selected');
            }
            example_sel.append(sel_item);
            $(this).addClass('example');
            obj = null;
            parentElement = null;
            caption = null;
        });
        $(this).prepend(example_sel);
        example_sel = null;
        i = null;
    });

    /* Display selected option code block */
    $('div.example-code ul.example-selector li').click(function(evt) {
        evt.preventDefault();
        $('ul.example-selector li').removeClass('selected');
        var sel_class = $(this).attr('class').replaceAll(' ', '.');
        $('div.example').hide();
        $('div.' + sel_class + ".example").show();
        $('div.code-block-caption.' + sel_class + '.example').show();
        $('ul.example-selector li.' + sel_class).addClass('selected');
        sel_class = null;
    });
});
