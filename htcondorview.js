function HTCondorView(id, url, graph_args, table_args, select_tuple) {
	"use strict";
	var that = this;
	$(document).ready(function() { that.initialize(id,url,graph_args,table_args,select_tuple); });
}


HTCondorView.prototype.initialize = function(id, url, graph_args, table_args, select_tuple) {
	"use strict";
	this.urlTool = document.createElement('a');
	var mythis = this;
	var i;

	url = "url="+url+"&";
	if(graph_args) { graph_args = url + graph_args; }
	if(table_args) { table_args = url + table_args; }

	var container = $('#'+id);
	if(container.length === 0) {
		console.log('HTCondor View is not able to intialize. There is no element with an ID of "'+id+'".');
		return false;
	}
	container.html(this.starting_html(!!table_args));

/*
	window.onpopstate = function() {
		setTimeout(function(){
			mythis.load_arguments_to_form();
			mythis.load_and_render(mythis.current_graphargs, mythis.current_tableargs);
			},1);
	}
	*/

	$('.download-link').click(function(ev) { mythis.download_csv(mythis.data.value); ev.preventDefault();});


	//this.load_arguments_to_form();
	//this.change_view()
	this.current_graphargs = graph_args;
	this.current_tableargs = table_args;
	if(select_tuple) {
		this.select_tuple = {};
		for(i = 0; i < select_tuple.length; i++) {
			this.select_tuple[select_tuple[i]] = true;
		}
	}
	this.load_and_render(this.current_graphargs, this.current_tableargs);
};

HTCondorView.next_graph_id = 0;

HTCondorView.prototype.new_graph_id = function() {
	"use strict";
	HTCondorView.next_graph_id++;
	var new_id = "htcondorview-private-" + HTCondorView.next_graph_id;
	return new_id;
};

HTCondorView.prototype.toggle_edit = function(btn, controls) {
	"use strict";
	$(controls).toggle();
	if($(controls).is(":visible")) {
		$(btn).text("❌");
	} else {
		$(btn).text("edit");
	}
};

/*
HTCondorView.prototype.load_arguments_to_form = function() {
	"use strict";
	var args = afterquery.parseArgs(window.location.search);

	var source = args.get('source');
	if(source === undefined) { source = 'submitters'; }
	$('#data-source input[type="radio"][value="'+source+'"]').prop('checked', true);

	var duration = args.get('duration');
	if(duration === undefined) { duration = 'day'; }
	$('#data-duration input[type="radio"][value="'+duration+'"]').prop('checked', true);

	var filterstr = args.get('filter');
	if(filterstr !== undefined) {
		var filterlist = filterstr.split(';');
		this.active_filter = {};
		for(var i = 0; i < filterlist.length; i++) {
			if(filterlist[i].length == 0) { continue; }
			var pair = filterlist[i].split('=');
			this.active_filter[pair[0]] = pair[1];
		}
	} else {
		this.active_filter = undefined;
	}

	var title = args.get('title');
	if(title !== undefined) { this.alt_title = title; }
}
*/

HTCondorView.prototype.replace_search_arg = function(oldurl, newkey, newval) {
	"use strict";
	this.urlTool.href = oldurl;
	var oldsearch = this.urlTool.search;
	var args = afterquery.parseArgs(oldsearch);
	var newsearch = "?";
	for(var argi in args.all) {
		var arg = args.all[argi];
		var key = arg[0];
		if(key.length && key != newkey) {
			var val = arg[1];
			newsearch += encodeURIComponent(key) + "=" + encodeURIComponent(val) + "&";	
		}
	}
	if(newval !== undefined) {
		newsearch += encodeURIComponent(newkey) + "=" + encodeURIComponent(newval);	
	}
	this.urlTool.search = newsearch;
	return this.urlTool.href;
};

/*
HTCondorView.prototype.save_arguments_to_url = function() {
	"use strict";
	var url = window.location.href;

	var source = $('#data-source input[type="radio"]:checked').val()
	var duration = $('#data-duration input[type="radio"]:checked').val()

	url = this.replace_search_arg(url, 'source', source);
	url = this.replace_search_arg(url, 'duration', duration);

	var filter;
	if(this.active_filter !== undefined) {
		filter = '';
		for(var key in this.active_filter) {
			filter = filter + key + "=" + this.active_filter[key] + ";";
		}
		filter = encodeURI(filter);
	}
	url = this.replace_search_arg(url, 'filter', filter);

	url = this.replace_search_arg(url, 'title', this.alt_title);

	if(url != window.location.href) {
		history.pushState(null, null, url);
	}
}
*/

/*
HTCondorView.prototype.read_arguments = function(source) {
	"use strict";
  var out = [];
  var parts = $(source).val().trim().split('\n');
  for (var parti in parts) {
    var part = parts[parti];
    if (part) {
      var bits = afterquery.internal.trySplitOne(part, '=');
      if (bits) {
        out.push(encodeURIComponent(bits[0]) + '=' + encodeURIComponent(bits[1]));
      } else {
        out.push(encodeURIComponent(part));
      }
    }
  }
  return  '?' + out.join('&');
}
*/

HTCondorView.prototype.load_and_render = function(graphargs, tableargs) {
	"use strict";

	/*
	this.save_arguments_to_url();
	*/

	var mythis = this;
	var callback_render_table = function() {
		$('#'+mythis.table_id+' .vizchart').empty();
		setTimeout(function() {
			var options = {
				select_handler: function(e,t,d){ mythis.table_select_handler(e,t,d); },
				num_pattern: '#,##0.0',
				disable_height: true
			};

			afterquery.render(tableargs, mythis.data.value, null, mythis.table_id, options);
		}, 0);
	 };
	if(tableargs === this.last_tableargs) { callback_render_table = null; }
	if(!tableargs) { callback_render_table = null; }
	this.last_tableargs = tableargs;

	var callback_render_graph = function(){
		$('#'+mythis.graph_id+' .vizchart').empty();
		setTimeout(function() {
			afterquery.render(graphargs, mythis.data.value, callback_render_table, mythis.graph_id);
			},0);
		};

	if(!graphargs) {
		if(callback_render_table) {
			callback_render_graph = callback_render_table; 
		} else {
			return; // Nothing to do.
		}
	}

	var args = graphargs;
	var newurl = afterquery.parseArgs(args).get('url');
	if(newurl == this.data_url) {
		callback_render_graph();
	} else {
		this.data_url = newurl;
		this.data = afterquery.load(args, null, function(){
			callback_render_graph();
			}, this.graph_id);
	}
};

HTCondorView.prototype.table_select_handler = function(evnt,table,data) {
	"use strict";
	var selection = table.getSelection();
	if(!selection) { return; }

	var col;
	var filter = "";

	var row = selection[0].row;

	for(col = 0; col < data.getNumberOfColumns(); col++) {
		var label = data.getColumnLabel(col);
		if(this.select_tuple[label]) {
			var value = data.getValue(row, col);
			filter += "filter=" + label + "=" + value + "&";
		}
	}

	var graphargs = filter + this.current_graphargs;
	// TODO: Rewrite the title for the active select_tuple.


	this.load_and_render(graphargs, this.current_tableargs);
};

HTCondorView.prototype.html_for_graph = function(id, myclass) {
	"use strict";
	return "" +
		"<div id='"+id+"' class='"+myclass+"'>\n" +
		"<div class='vizstatus'>\n" +
		"  <div class='statustext'></div>\n" +
		"  <div class='statussub'></div>\n" +
		"</div>\n" +
		"<div class='vizraw'></div>\n" +
		"<div class='vizchart'></div>\n" +
		"</div>\n";
};

HTCondorView.prototype.starting_html = function(has_table) {
	"use strict";
	this.graph_id = this.new_graph_id();
	this.table_id = this.new_graph_id();

	var ret = "" +
	'<div class="htcondorview">\n' +
	  "<div class='editmenu'>\n" +
	    "<button onclick=\"alert('Not yet implemented')\" class=\"editlink\">full screen</button>\n" +
	  "</div>\n" +
	  this.html_for_graph(this.graph_id, "graph")+ "\n" +
	"\n";
	if(has_table) {
		ret += "<div class=\"download-link\"> <a href=\"#\">Download this table</a> </div>\n" +
		this.html_for_graph(this.table_id, "table")+ "\n";
	}
	ret += "</div>\n";

	return ret;
};


HTCondorView.prototype.afterquerydata_to_csv = function(dt) {
	"use strict";
	function csv_escape(instr) {
		instr = "" + instr;
		if((instr.indexOf('"') == -1) &&
			(instr.indexOf(',') == -1) &&
			(instr.charAt[0] != ' ') &&
			(instr.charAt[0] != "\t") &&
			(instr.charAt[instr.length-1] != " ") &&
			(instr.charAt[instr.length-1] != "\t"))
		{
			// No escaping necessary
			return instr;
		}
		instr = instr.replace(/"/g, '""');
		return '"' + instr + '"';
	}
	var ret = '';
	var columns = dt.headers.length;
	var rows = dt.data.length;

	var col;

	for(col = 0; col < columns; col++) {
		if(col > 0) { ret += ","; }
		ret += csv_escape(dt.headers[col]);
	}
	ret += "\n";

	for(var row = 0; row < rows; row++) {
		for(col = 0; col < columns; col++) {
			if(col > 0) { ret += ","; }
			ret += csv_escape(dt.data[row][col]);
		}
		ret += "\n";
	}

	return ret;
};

HTCondorView.prototype.download_data = function(filename, type, data) {
	"use strict";
	var link = document.createElement('a');
	link.download = filename;
	link.href = "data:"+type+";charset=utf-8,"+encodeURIComponent(data);
	document.body.appendChild(link);
	link.click();
	document.body.removeChild(link);
};

HTCondorView.prototype.download_csv = function(data) {
	"use strict";
	var mythis = this;
	var handle_csv = function() {
		var csv = mythis.afterquerydata_to_csv(mythis.csv_source_data.value);
		mythis.csv_source_data = undefined;
		mythis.download_data("HTCondor-View-Data.csv", "text/csv", csv);
	};
	this.csv_source_data = afterquery.load_post_transform(this.current_tableargs, data, handle_csv, null);
};



