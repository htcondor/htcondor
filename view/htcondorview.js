/*

HTCondorView

REQUIREMENTS (<head>):
  <link rel="stylesheet" href="htcondorview.css" />
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/1.11.3/jquery.js"></script>
  <script src="https://ajax.googleapis.com/ajax/libs/jqueryui/1.11.4/jquery-ui.min.js"></script> 
  <script src="https://cdnjs.cloudflare.com/ajax/libs/dygraph/1.1.0/dygraph-combined.js"></script>
  <script src="https://www.google.com/jsapi"></script>
  <script>
  google.load('visualization', '1.0', {
    packages: ['table', 'corechart', 'treemap', 'annotatedtimeline']
  });
  </script>
  <script src="htcondorview.js"></script>

SIMPLE USAGE:

	<script>
	HTCondorView.simple("url=submitters.now.json&title=Total Jobs&order=JobStatus&group=JobStatus;Count&chart=pie");
	</script>

This will insert a <div> whereever the script appears and place a chart inside, as described by the single argument which is an Afterquery query.  The <div> will have a class of "HTCondorViewSimple" and a unique, arbitrary ID.

Returns an HTCondorView object, which is not currently useful.


COMPLEX USAGE:

	<div id="myexample"></div>

	<script>
	new HTCondorView({
			dst_id: "myexample",
			data_url: "submitters..json",
			date_start: new Date("2015-01-01T00:00:00"),
			date_end: new Date("2016-01-01T00:00:00"),
			graph_query: "order=Date&pivot=Date;JobStatus;Count&chart=stacked",
			title: "Total Jobs",
			table_query: "order=Date&pivot=Name;JobStatus;avg(Count)",
			select_tuple: ["Name"],
			has_fullscreen_link: true
		});
	</script>

dst_id - The chart will be placed inside the <div> or other element with this id.  Previous contents will be destroyed.

data_url - URL, possibly relative, to the data to be loaded.  If the URL contains "..", AND date_start is present, the date (ex "2016-04") or the word "oldest" will be inserted into the first ".." in the URL as necessary to satisfy the date range.  Otherwise the URL is loaded exactly as is.

date_start - Optional. Date object.  The start of the range to download and display. If present, the data_url MUST contain "..".

date_end - Optional. Date object. If present, date_start is mandatory.  The end of the range to download and display.  If date_start is specified, but not date_end, date_end is assumed to be "now" (ie "new Date()").

graph_query - Afterquery query, but not including the title or url entries.  This is the chart to display.

title - Optional. A title to display with the chart.

table_query - Optional. Afterquery query, not including title or url entries.  This specifies a second chart to display.  Probably should be a simple table (that is, don't specify "chart").  If not present, no second chart is present.

select_tuple - Optional. If table_query is present and is a table, when a user clicks on a row in the table, these fields will be extracts from the selected row and used to filter the graph specified in graph_query.

has_fullscreen_link: true/false - Optional. Defaults to true. If false (and exactly false, not 0), the link to show the graph fullscreen will be suppressed.


*/


function HTCondorView(id, url, graph_args, options) {
	"use strict";
	var that = this;

	var options_obj;
	if(typeof(id) === 'object') {
		options_obj = id;

	} else {
		// This is a (hopefully) a raw afterquery query.
		var args = AfterqueryObj.parseArgs(url);
		options_obj = {
			dst_id: id,
			title: args.get("title"),
			data_url: args.get("url"),
			graph_query: "",
		};

		for(var argi in args.all) {
			var arg = args.all[argi];
			var key = arg[0];
			if(key.length && key !== "title" && key !== "url") {
				var val = arg[1];
				options_obj.graph_query += encodeURIComponent(key) + "=" + encodeURIComponent(val) + "&";	
			}
		}
	}

	var ready_datefiles;
	if(options_obj.data_url.search(/\.\./) != -1) {
		this.datefiles = new HTCondorViewDateFiles(options_obj.data_url);
		ready_datefiles = this.datefiles.promise();
		ready_datefiles.fail(function(){
			console.log("Failed to initialize HTCondorViewDateFiles");
		});
	} else {
		ready_datefiles = $.Deferred();
		ready_datefiles.resolve();
	}

	var ready_document = $.Deferred();
	$(document).ready(function() { ready_document.resolve(); });

	$.when(ready_datefiles, ready_document).then(function(){
		that.initialize(options_obj);
	});
		
}

HTCondorView.simple = function(query, thisclass) {
	"use strict";
	var newid = HTCondorView.prototype.new_graph_id();
	if(thisclass === null || thisclass === undefined) { 
		thisclass = "HTCondorViewSimple";
	}
	var tag = '<div id="'+newid+'" class="'+thisclass+'"></div>';
	document.write(tag);
	return new HTCondorView(newid, query);
};

HTCondorView.prototype.initialize = function(options) {
	"use strict";

	if(typeof(options) !== 'object') { options = {}; }


	var id = options.dst_id;
	var graph_args = options.graph_query;
	var table_args = options.table_query;
	var select_tuple = options.select_tuple;
	this.urlTool = document.createElement('a');
	var that = this;
	var i;

	this.date_start = options.date_start;
	this.date_end = options.date_end;
	if(this.date_start && !this.date_end) { this.date_end = new Date(); }
	else if(this.date_end && !this.date_start) { this.date_end = null; }

	this.original_title = options.title;
	this.title = options.title;
	this.url = options.data_url;

	var container = $('#'+id);
	if(container.length === 0) {
		console.log('HTCondor View is not able to intialize. There is no element with an ID of "'+id+'".');
		return false;
	}
	container.empty();

	var has_fullscreen_link = true;
	if(options.has_fullscreen_link === false) {
		has_fullscreen_link = false;
	}
	var starting_elements = this.starting_elements({
		has_table: !!table_args,
		has_fullscreen_link: has_fullscreen_link
	});

	this.aq_table = new AfterqueryObj({root_id: this.table_id});
	this.aq_total_table = new AfterqueryObj({root_id: this.total_table_id});
	this.aq_graph = new AfterqueryObj({root_id: this.graph_id});

	container.append(starting_elements);
	this.graph_fullscreen_link = $('#'+this.graph_fullscreen_id);
	this.graph_edit_link = $('#'+this.graph_edit_id);

	this.table_fullscreen_link = $('#'+this.table_fullscreen_id);
	this.table_edit_link = $('#'+this.table_edit_id);

	$('#'+this.table_download_id).click(function(ev) { that.download_csv(that.data, that.current_tableargs); ev.preventDefault();});

	$('#'+this.graph_download_id).click(function(ev) { that.download_csv(that.data, that.current_graphargs); ev.preventDefault();});

	var date_filter = '';

	if(this.date_start) {
		console.log(this.date_start,"-", this.date_end);
		date_filter += "filter=Date>="+this.date_start.toISOString() + "&";
		date_filter += "filter=Date<"+this.date_end.toISOString() + "&";
	}

	if(graph_args) { graph_args = date_filter + graph_args; }
	if(table_args) { table_args = date_filter + table_args; }

	//this.change_view()
	this.starting_graphargs = graph_args;
	this.set_graph_query(graph_args);
	this.set_table_query(table_args);
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

HTCondorView.prototype.set_graph_query = function(graph_query) {
	"use strict";
	this.current_graphargs = graph_query;
	var search = '?';
	if(this.title) {
		search += "title=" + encodeURIComponent(this.title) + "&";	
	}
	search += "data_url=" + encodeURIComponent(this.url) + "&";	
	if(this.date_start) {
		search += "date_start=" + encodeURIComponent(this.date_start.toISOString()) + "&";	
	}
	if(this.date_end) {
		search += "date_end=" + encodeURIComponent(this.date_end.toISOString()) + "&";	
	}
	search += "graph_query=" + encodeURIComponent(this.current_graphargs) + "&";	
	if(this.graph_fullscreen_link) {
		this.graph_fullscreen_link.attr('href', "fullscreen.html" + search);
	}

	if(this.graph_edit_link) {
		this.graph_edit_link.attr('href', "edit.html" + search);
	}
};

HTCondorView.prototype.set_table_query = function(table_query) {
	"use strict";
	this.current_tableargs = table_query;
	var search = '?';
	if(this.title) {
		search += "title=" + encodeURIComponent(this.title) + "&";	
	}
	search += "data_url=" + encodeURIComponent(this.url) + "&";	
	search += "graph_query=" + encodeURIComponent(this.current_tableargs) + "&";	
	if(this.table_fullscreen_link) {
		this.table_fullscreen_link.attr('href', "fullscreen.html" + search);
	}

	if(this.table_edit_link) {
		this.table_edit_link.attr('href', "edit.html" + search);
	}
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


HTCondorView.prototype.replace_search_arg = function(oldurl, newkey, newval) {
	"use strict";
	this.urlTool.href = oldurl;
	var oldsearch = this.urlTool.search;
	var args = AfterqueryObj.parseArgs(oldsearch);
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

HTCondorView.prototype.total_table_args = function(headers, select_tuble) {
	"use strict";
	var fields = [];
	var i; 
	for(i = 0; i < headers.length; i++) {
		if( ! this.select_tuple[headers[i]]) {
			fields.push(headers[i]);
		}
	}
	return "group=;"+fields.join(",");
};


// Given a data grid in Afterquery format, prepend a new field
// with a header of "", type of T_STRING, and all values of "TOTAL".
HTCondorView.prototype.add_total_field = function(grid) {
	"use strict";
	grid.headers.unshift('');
	grid.types.unshift(AfterqueryObj.T_STRING);
	var i;
	for(i = 0; i < grid.data.length; i++) {
		grid.data[i].unshift('TOTAL');
	}
	return grid;
};

HTCondorView.prototype.aq_load = function(url, start, end) {
	var def = $.Deferred();
	var that = this;

	var files = [url];

	if(start) {
		files = this.datefiles.get_files(start, end);
	}

	var query = "url="+files.join("&url=");

	if(query === this.data_id) {
		def.resolve(this.data);
		return def;
	} else {
		this.data_id = query;
		this.aq_graph.load(query, null, function(data){
			that.data = data;
			def.resolve(data);
			});
	}
	return def.promise();
};

HTCondorView.prototype.promise_render_viz = function(viz, args) {
	"use strict";
	var def = $.Deferred();
	$('#'+args.id+' .vizchart').empty();
	var query_bits = [];
	if(args.url) { query_bits.push("url="+args.url); }
	if(args.query_args) { query_bits.push(args.query_args); }
	if(args.title) { query_bits.push("&title="+ encodeURIComponent(args.title)); }
	var options = {
		num_pattern: '#,##0.0',
		disable_height: true
	};
	if(args.select_handler) {
		options.select_handler = args.select_handler;
	}
	var query = query_bits.join("&");
	viz.render(query, args.data, function(data){def.resolve(data);}, options);
	return def.promise();
};

HTCondorView.prototype.callback_transform_total_table = function(tableargs, data) {
	"use strict";
	var def = $.Deferred();
	var query = this.total_table_args(data.headers, tableargs, this.select_tuple);
	this.aq_total_table.load_post_transform(query, data, function(d){def.resolve(d);});
	return def.promise();
};

HTCondorView.prototype.load_and_render = function(graphargs, tableargs) {
	"use strict";
	var that = this;

	var promise_data_loaded = this.aq_load(that.url, that.date_start, that.date_end);
	var promise;

	var graph_id = this.graph_id;
	var table_id = this.table_id;
	var total_table_id = this.total_table_id;
	var url = this.url;
	var title = this.title;

	if(graphargs) {
		promise_data_loaded.then(function(data){
			return that.promise_render_viz(that.aq_graph, {
				id: graph_id,
				url: url,
				title: title,
				query_args: graphargs,
				data: data
				});
			});
	}
	if(tableargs && tableargs !== this.last_tableargs) {
		that.last_tableargs = tableargs;
		promise = promise_data_loaded.then(function(data){
			return that.promise_render_viz(that.aq_table, {
				id: table_id,
				url: url,
				query_args: tableargs,
				select_handler: function(e,t,d){ that.table_select_handler(e,t,d); },
				data: data
				});
			});
		promise = promise
			.then(function(d){return that.callback_transform_total_table(tableargs,d);})
			.then(function(d){return that.add_total_field(d);})
			.then(function(data){
				return that.promise_render_viz(that.aq_total_table, {
					id: total_table_id,
					url: url,
					select_handler: function(e,t,d){ that.total_table_select_handler(e,t,d); },
					data: data
					});
				});
	}
};

HTCondorView.prototype.total_table_select_handler = function(evnt,table,data) {
	"use strict";
	var selection = table.getSelection();
	if(!selection) { return; }
	if(this.table_obj) {
		this.table_obj.setSelection(null);
	}

	this.total_table_obj = table;

	this.title = this.original_title;

	this.set_graph_query(this.starting_graphargs);

	this.load_and_render(this.current_graphargs, this.current_tableargs);
};

HTCondorView.prototype.table_select_handler = function(evnt,table,data) {
	"use strict";
	var selection = table.getSelection();
	if(!selection) { return; }

	if(this.total_table_obj) {
		this.total_table_obj.setSelection(null);
	}

	this.table_obj = table;

	var col;
	var filter = "";

	var row = selection[0].row;

	this.title = this.original_title + " for ";
	var first_tuple = true;

	for(col = 0; col < data.getNumberOfColumns(); col++) {
		var label = data.getColumnLabel(col);
		if(this.select_tuple[label]) {
			var value = data.getValue(row, col);
			filter += "filter=" + label + "=" + value + "&";
			if(!first_tuple) { this.title += "/"; }
			first_tuple = false;
			this.title += value;
		}
	}

	this.set_graph_query(filter + this.starting_graphargs);

	this.load_and_render(this.current_graphargs, this.current_tableargs);
};

HTCondorView.prototype.html_for_graph = function() {
	"use strict";
	return "" +
		"<div class='vizstatus'>\n" +
		"  <div class='statustext'></div>\n" +
		"  <div class='statussub'></div>\n" +
		"</div>\n" +
		"<div class='vizraw'></div>\n" +
		"<div class='vizchart'></div>\n" +
		"<div class='vizlog'></div>\n";
};

HTCondorView.prototype.starting_elements = function(options) {
	"use strict";

	var has_table = options.has_table;
	var has_fullscreen_link = options.has_fullscreen_link;

	this.graph_id = this.new_graph_id();
	this.table_id = this.new_graph_id();
	this.total_table_id = this.new_graph_id();
	this.graph_fullscreen_id= this.new_graph_id();
	this.graph_edit_id= this.new_graph_id();
	this.graph_download_id= this.new_graph_id();

	this.table_fullscreen_id= this.new_graph_id();
	this.table_edit_id= this.new_graph_id();
	this.table_download_id= this.new_graph_id();

	function editmenu(fullscreen_id, edit_id, download_id) {
		var ret = "<div class='editmenu'>\n";
		if(has_fullscreen_link) {
			ret += "<a href='#' id='"+fullscreen_id+"' class=\"editlink\">full screen</a><br>\n";
		}
		ret += "<a href='#' id='"+download_id+"' class=\"editlink\">download data</a><br>\n";
		ret += "<a href='#' id='"+edit_id+"' class=\"editlink\">edit</a><br>\n";
		ret += "</div>\n";
		return ret;
	}


	var ret = "" +
		'<div class="htcondorview">\n' +
		  "<div id='"+this.graph_id+"' class='graph'>\n" +
			editmenu(this.graph_fullscreen_id, this.graph_edit_id, this.graph_download_id) +
	        this.html_for_graph() + "\n"+
		  "</div>\n";
	if(has_table) {
		ret += "<div id='"+this.total_table_id+"' class='table'>\n" +
			this.html_for_graph()+ "\n" +
			"</div>\n";
		ret += "<div id='"+this.table_id+"' class='table'>\n" +
			editmenu(this.table_fullscreen_id, this.table_edit_id, this.table_download_id) +
			this.html_for_graph()+ "\n" +
			"</div>\n";
	}
	ret += "</div>\n";

	var element = $(ret);

	return element;
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
			var val = dt.data[row][col];
			if(typeof(val) === 'number' && isNaN(val)) {
				val = '';
			}
			ret += csv_escape(val);
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

HTCondorView.prototype.download_csv = function(data, query) {
	"use strict";
	var that = this;
	var handle_csv = function() {
		var csv = that.afterquerydata_to_csv(that.csv_source_data.value);
		that.csv_source_data = undefined;
		that.download_data("HTCondor-View-Data.csv", "text/csv", csv);
	};
	this.csv_source_data = that.aq_table.load_post_transform(query, data, handle_csv);
};


HTCondorView.input_date = function(name, id) {
	return '<input type="date" placeholder="YYYY-MM-DD" title="Use YYYY-MM-DD format.\rFor example, use 2015-06-01 for June 1, 2015" pattern="\\d\\d\\d\\d-\\d\\d?-\\d\\d?" class="range_input datepicker" name="'+name+'" id="'+id+'">';
}

HTCondorView.input_time = function(name, id) {
	return '<input type="time" placeholder="HH:MM" title="Use HH:MM format.\rFor example, use 13:00 for 1:00 PM" pattern="\\d+:\\d\\d(:\\d\\d(\\.\\d\\d\\d)?)?\\s*([aApP][mM])?" class="range_input" name="'+name+'" id="'+id+'">';
}


////////////////////////////////////////////////////////////////////////////////

/*
HTCondorViewDateFiles is an internal-to-HTCondorView object for determining
which data files we need to load to display a given range at an acceptable
level of detail.


base_url - Mandatory. URL (relative or absolute) to the data files.  The
URL must contain the string "..", which indicates where ".oldest." or
".DATE." will be placed.

ready_func - Optional. If present, called when the object is ready. Will be
passed a copy of "this".  The delay is because the object needs to load the
".oldest." file before it can field queries.

fail_func - Optional. If present, called if the object is unable to
initialize.  Will be passed a copy of "this".  Will fail if the ".oldest."
file could not be loaded.
*/
function HTCondorViewDateFiles(base_url, ready_func, fail_func) {
	this.base_url = base_url;
	this.my_promise = jQuery.Deferred();

	if(ready_func) { this.my_promise.done(ready_func); }
	if(fail_func) { this.my_promise.fail(fail_func); }

	var oldest_url = HTCondorViewDateFiles.expand(base_url, "oldest");
	var that = this;
	$.ajax(oldest_url, {dataType:'json'}).then(
		function(data){that.oldest_loaded(data);},
		function(j,t,e){that.failed_load(j, t, e);}
		);
}

/*
Returns a jQuery.promise which triggers when the object is ready to
field queries, or upon failing to load the ".oldest." file (and thus
will never be ready to field queries)
*/
HTCondorViewDateFiles.prototype.promise = function() {
	return this.my_promise.promise();
};


HTCondorViewDateFiles.expand = function(base_url, content) {
	return base_url.replace("..", "."+content+".");
};


HTCondorViewDateFiles.prototype.oldest_loaded = function(oldest) {
	this.oldest = {};
	var keys = Object.keys(oldest);
	var key;
	var val;
	var dateval;
	for (var i = 0; i < keys.length; i++) {
		key = keys[i];
		val = oldest[key];
		dateval = Date.parseMore(val);
		this.oldest[key] = dateval;
		//console.log(val," -> ", dateval.toUTCString());
		if((!this.absolute_oldest) || this.absolute_oldest.getTime() > dateval.getTime()) {
			this.absolute_oldest = dateval;
		}
	}
	//console.log("Oldest available data", this.oldest);
	//console.log("Absolute oldest:", this.absolute_oldest);
	this.my_promise.resolve(this);
};

HTCondorViewDateFiles.prototype.failed_load = function(jqXHR, textStatus, errorThrown) {
	console.log("failed to load", this.base_url);
	console.log(jqXHR, textStatus, errorThrown);
	this.my_promise.reject(this);
};

HTCondorViewDateFiles.truncate_to_day = function(dateobj) {
	// TODO: probably faster to set individual fields to 0.
	return Date.parseMore(dateobj.getUTCISOYearMonthDay());
}

HTCondorViewDateFiles.truncate_to_week = function(dateobj) {
	// TODO: There is probably a faster solution, but it may
	// be complex.
	var w = dateobj.getUTCISOWeekDate();
	var r = Date.parseMore(w);
	var tmp = r.getUTCISOWeekDate();
	//console.log("        ", dateobj,"->", w, "->", r, "(",tmp,")");
	return r;
}

HTCondorViewDateFiles.truncate_to_month = function(dateobj) {
	// TODO: probably faster to set individual fields to 0.
	return Date.parseMore(dateobj.getUTCISOYearMonth());
}

HTCondorViewDateFiles.prototype.get_files_aide = function(label, start, end, step, truncater, formatter) {
//console.log("    ", start, end, step, truncater, formatter);
//console.log("  start", start);
	var now = truncater(start);
//console.log("  trunc", now);
//console.log("    end", end, "+",step,"- 1");
	end.setTime(end.getTime()+step-1);
//console.log("      =", end);
	end = truncater(end);
//console.log("  trunc", end);

	var retfiles = [];
	var stamp;

	while(now.getTime() < end.getTime()) {
//console.log("    ", now, end);
		stamp = formatter(now);
		retfiles.push(stamp);
		now.setTime(now.getTime() + step);
//console.log("       ", now, end);
	}

	var ret_urls = [];
	for(var i = 0; i < retfiles.length; i++) {
		var insert = label + "." + retfiles[i];
		var expanded = HTCondorViewDateFiles.expand(this.base_url, insert);
		ret_urls.push(expanded);
	}

	return ret_urls;
}

HTCondorViewDateFiles.prototype.get_files = function(start,end) {
	start = Date.parseMore(start);
	end = Date.parseMore(end);

	if(start.getTime() < this.absolute_oldest.getTime()) {
		start = this.absolute_oldest;
	}
	if(start.getTime() > end.getTime()) { return []; }

	//console.log("considering", start, "to", end);

	var day = 1000*60*60*24;
	var week = day*7;
	var month = day*31;

	var delta = end.getTime() - start.getTime();

	var stamp;
	var retfiles = [];


	var now;

	// We choose the smallest interval that covers the time period
	// and is at least as long as the time period.
	// Goal is to load no more than 2 files unless there are no other
	// options. In addition, which granularity of files we load should
	// always be the same for a given duration; we don't want to jump
	// between daily and weekly files because we're loading 36 hours and
	// sometimes we'd need 2 daily and sometimes 3.

	if( delta <= day &&
		this.oldest.daily &&
		start.getTime() >= this.oldest.daily.getTime()) {
		retfiles = this.get_files_aide("daily", start, end, day,
			HTCondorViewDateFiles.truncate_to_day,
			function(d) { return d.getUTCISOYearMonthDay(); }
			);
		console.log("  daily:", retfiles);
		return retfiles;
	}

	if( delta <= week &&
		this.oldest.weekly &&
		start.getTime() >= this.oldest.weekly.getTime()) {
		retfiles = this.get_files_aide("weekly", start, end, week,
			HTCondorViewDateFiles.truncate_to_week,
			function(d) { return d.getUTCISOWeekDate(); }
			);
		console.log("  weekly:", retfiles);
		return retfiles;
	}

	retfiles = this.get_files_aide("monthly", start, end, month,
		HTCondorViewDateFiles.truncate_to_month,
		function(d) { return d.getUTCISOYearMonth(); }
		);
	console.log("  monthly:", retfiles);
	return retfiles;
};


////////////////////////////////////////////////////////////////////////////////

/*
HTCondorViewRanged provides the same constructor interface as HTCondorView.
In addition, it addres a Date|Week|Month|Custom UI to select what date ranges
are displayed.  The provided data_url _must_ contain "..", which will be 
replaced with the interval and start date as appropriate.  Data must have a 
"Date" field.
*/


function HTCondorViewRanged(options) {
	"use strict";
	var that = this;
	this.options = options;
	$(document).ready( function() { that.initialize(); });
}


HTCondorViewRanged.prototype.new_graph_id = function() {
	"use strict";
	return HTCondorView.prototype.new_graph_id();
};


// If the browser doesn't provide a native UI for <input type=date>,
// use jQueryUIs. This replaces everything with class="datepicker".
HTCondorViewRanged.prototype.add_date_pickers = function(root_id) {
	if( HTCondorViewRanged.has_native_date !== false && 
		HTCondorViewRanged.has_native_date !== true) {

		HTCondorViewRanged.has_native_date = false;
		try {
			// This test is based on the design from inputtypes.js in Modernizr, released under an MIT license. This is a reimplementation. If we every need more Modernizr tests, we should probably just use the real library.
			var test_input_e = $('<input>');
			test_input_e.hide();
			$('body').append(test_input_e);
			test_input_e.attr('type', 'date');
			if(test_input_e.attr('type') === 'text') {
				test_input_e.remove();
				throw "input type='date' is not supported";
			}
			test_input_e[0].value = "1)";
			if(test_input_e[0].value === "1)") {
				test_input_e.remove();
				throw "input type='date' is not supported";
			}
			test_input_e.remove();
			HTCondorViewRanged.has_native_date = true;
			native_date = true;
		} catch(e) {
			// Something went wrong. We don't care what, just fallback to whatever the browser provides.
		}
	}
	if(! HTCondorViewRanged.has_native_date ) {
		// We clear the style so autoSize can work.
		$('#'+root_id+' .datepicker').removeClass("range_input");
		// TODO: we could set minDate/maxDate to the range of data we know we have.
		$('#'+root_id+' .datepicker').datepicker({
			dateFormat: 'yy-mm-dd',
			autoSize: true
		});
	}
};

HTCondorViewRanged.prototype.initialize = function() {
	"use strict";
	var that = this;
	this.dst_id = this.options.dst_id;
	var container = $('#'+this.dst_id);
	if(container.length === 0) {
		console.log('HTCondorViewRanged is not able to intialize. There is no element with an ID of "'+this.dst_id+'".');
		return;
	}
	var new_html = this.html_tabs();
	container.html(new_html);
	this.add_date_pickers(this.dst_id);


	this.options.dst_id = this.graph_id;

	$('#'+this.id_range).hide();

	// Initialize tabs
	$('#'+this.dst_id+' ul.tabs li').click(function(){
		$('#'+this.dst_id+' ul.tabs li').removeClass('current');
		$(this).addClass('current');
	});
	$('#'+this.dst_id+' ul.radio-tabs input').change(function() {
		that.change_view();
		});

	$('#'+this.dst_id+' button.update_range').click(function() {
		that.change_view();
		});

	this.change_view();

};


HTCondorViewRanged.prototype.change_view = function() {
	// Purge everything.
	//$("#"+this.graph_id).empty();
	//this.htcondor_view = null;

	var duration = $("#"+this.dst_id+' .data-duration input[type="radio"]:checked').val();

	var options = {};
	for(key in this.options) {
		options[key] = this.options[key];
	}
	var now = new Date(Date.now());
	var start = new Date(Date.now());
	if(duration === "day") {
		start.setTime(start.getTime() - 1000*60*60*24);
		options.date_start = start;
		options.date_end = now;
		$('#'+this.id_range).hide();
	} else if(duration === "week") {
		start.setTime(start.getTime() - 1000*60*60*24*7);
		options.date_start = start;
		options.date_end = now;
		$('#'+this.id_range).hide();
	} else if(duration === "month") {
		start.setTime(start.getTime() - 1000*60*60*24*31);
		options.date_start = start;
		options.date_end = now;
		$('#'+this.id_range).hide();
	} else if(duration === "custom") {
		$('#'+this.id_range).show();
		options.date_start = Date.parseDateTime(
			$('#'+this.id_start_date).val(),
			$('#'+this.id_start_time).val())
		options.date_end = Date.parseDateTime(
			$('#'+this.id_end_date).val(),
			$('#'+this.id_end_time).val())
		console.log(options.date_start, options.date_end);
		if(!options.date_start || (!options.date_end)) {
			//console.log("unparsable");
			return;
		}
		if(options.date_start.getTime() > options.date_end.getTime()) {
			//console.log("Backward range");
			return;
		}
	}
	
	this.htcondor_view = new HTCondorView(options);
};

HTCondorViewRanged.prototype.html_tabs = function() {
	// We need to ensure the IDs are all unique.
	var id_dd = this.new_graph_id();
	var id_dw = this.new_graph_id();
	var id_dm = this.new_graph_id();
	var id_dc = this.new_graph_id();
	this.id_start_date = this.new_graph_id();
	this.id_start_time = this.new_graph_id();
	this.id_end_date = this.new_graph_id();
	this.id_end_time = this.new_graph_id();

	this.id_range = this.new_graph_id();

	function html_radio(name, id, value, label, checked) {
		if(checked) { checked = " checked"; }
		else { checked = ""; }
		return "<input type='radio' name='"+name+"' id='"+id+"' value='"+value+"'"+checked+"> <label for='"+id+"'>"+label+"</label>";
	}

	this.graph_id = this.new_graph_id();
	return "" +
	"<div class='htcondorviewranged'>\n"+
		"<div class='tabs'>\n" +
			"<ul class='radio-tabs data-duration'>\n" +
				"<li>"+html_radio("data-duration", "data-duration-day-"+id_dd, "day", "Day", "checked") + "\n" +
				"<li>"+html_radio("data-duration", "data-duration-week-"+id_dw, "week", "Week") + "\n" +
				"<li>"+html_radio("data-duration", "data-duration-month-"+id_dm, "month", "Month") + "\n" +
				"<li>"+html_radio("data-duration", "data-duration-custom-"+id_dc, "custom", "Custom") + "\n" +
			"</ul>\n" +
		"</div>\n" +
		'<div class="range" id="'+this.id_range+'">\n' +
			HTCondorView.input_date('start_date', this.id_start_date) +"\n" +
			HTCondorView.input_time('start_time', this.id_start_time) +"\n" +
			'through\n' +
			HTCondorView.input_date('end_date', this.id_end_date) +"\n" +
			HTCondorView.input_time('end_time', this.id_end_time) +"\n" +
			"<button class='update_range'>Update chart</button>\n"+
		'</div>\n' +
		'<div id="'+this.graph_id+'"></div>\n' +
	'</div>\n';
};

////////////////////////////////////////////////////////////////////////////////
/*
Date manipulation extensions for HTCondorView

getUTCWeek and getUTCWeekYear are from:
van den Broek, Taco. Calculate ISO 8601 week and year in javascript. Procuios. 2009-04-22. URL:http://techblog.procurios.nl/k/news/view/33796/14863/calculate-iso-8601-week-and-year-in-javascript.html. Accessed: 2015-12-07. (Archived by WebCite® at http://www.webcitation.org/6dbdvnbtj)
Released and used under the MIT license.
*/

// Get ISO 8601 week number for a date.
Date.prototype.getUTCWeek = function () {
	"use strict";
	var target  = new Date(this.valueOf());
	var dayNr   = (this.getUTCDay() + 6) % 7;
	target.setUTCDate(target.getUTCDate() - dayNr + 3);
	var firstThursday = target.valueOf();
	target.setUTCMonth(0, 1);
	if (target.getUTCDay() != 4) {
		target.setUTCMonth(0, 1 + ((4 - target.getUTCDay()) + 7) % 7);
	}
	return 1 + Math.ceil((firstThursday - target) / 604800000);
}

// Get ISO 8601 year, assuming we're using week numbers.
Date.prototype.getUTCWeekYear = function () {
	"use strict";
	var target	= new Date(this.valueOf());
	target.setUTCDate(target.getUTCDate() - ((this.getUTCDay() + 6) % 7) + 3);
	return target.getUTCFullYear();
}

// Get ISO 8610 year and week in form "2016-W01"
Date.prototype.getUTCISOWeekDate = function ()  {
	"use strict";
	var week = this.getUTCWeek();
	week = ("0"+week).slice(-2); // Zero pad.
	var year = this.getUTCWeekYear();
	return year+"-W"+week;
}

Date.prototype.getUTCISOYearMonth = function() {
	"use strict";
	var year = this.getUTCFullYear();
	var month = this.getUTCMonth()+1;
	month = ("0"+month).slice(-2); // Zero pad.
	return year+"-"+month;
}

Date.prototype.getUTCISOYearMonthDay = function() {
	"use strict";
	var yearmonth = this.getUTCISOYearMonth();
	var day = this.getUTCDate();
	day = ("0"+day).slice(-2); // Zero pad.
	return yearmonth+"-"+day;
}

// Identical to "new Date(str)", but accepts ISO 8601 week dates (eg "2015-W04"). Dates are assumed to always be in UTC.
// Week dates will return midnight on the Monday morning of that week. 
Date.parseMore = function(str) {
	"use strict";
	var fields = /^(\d\d\d\d)-W(\d\d)$/.exec(str);
	if((!fields) || fields.length !== 3) { return new Date(Date.parse(str)); }
	var year = Number(fields[1]);
	var week = Number(fields[2]);

	// Based on http://stackoverflow.com/a/19375264/16672
	var d = new Date(Date.UTC(year, 0, 1));
	var week_in_ms = 1000*60*60*24*7;
	d.setUTCDate(d.getUTCDate() + 4 - (d.getUTCDay() || 7));
	d.setTime(d.getTime() + week_in_ms * (week + (year == d.getUTCFullYear() ? -1 : 0 )));
	d.setUTCDate(d.getUTCDate() - 3);
	return d;
}

Date.parseDateTime = function(date, time) {
	var RE_DATE = /(\d\d\d\d)[-\/](\d+)[-\/](\d+)/;

	var hits = RE_DATE.exec(date);
	if(!hits) { return; }
	var yyyy = parseInt(hits[1]);
	var mm = parseInt(hits[2]);
	var dd = parseInt(hits[3]);

	if(!time) { time = "00:00"; }
	var RE_TIME = /(\d+):(\d\d)(?::(\d\d)(?:\.(\d\d\d))?)?\s*([AP][M])?/i;
	hits = RE_TIME.exec(time);
	if(!hits) { return; }
	var hour = parseInt(hits[1]);
	var min = parseInt(hits[2]);
	var sec = parseInt(hits[3])||0;
	var millisec = parseInt(hits[4])||0;
	var ampm = hits[5];

	console.log("hour", hour, ampm);
	if(ampm) {
		if(ampm.match(/PM/i) && hour < 12) {
			hour += 12;
		} else if(ampm.match(/AM/i) && hour == 12) {
			hour -= 12;
		}
	}
	console.log("hour", hour, ampm);

	var ret_date = new Date(yyyy, mm-1, dd, hour, min, sec, millisec);
	if(isNaN(ret_date.getTime())) { return; }
	return ret_date;
}




////////////////////////////////////////////////////////////////////////////////
/* 
AfterqueryObj is a library for processing data and rendering graphs. 

 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * Additional modifications 2015 by Todd Tannenbaum and Alan De Smet, 
 * Center for High Throughput Computing, University of Wisconsin - Madison.
 * Licensed under the Apache License, Version 2.0.
 */
'use strict';

function AfterqueryObj(options) {
  if(options === null || options === undefined) {
    options = {};
  }

  this.root_id = options.root_id;

  this.colormap = {};
  this.next_color = 0;
};


AfterqueryObj.prototype.elid = function(id) {
    if(this.root_id) {

      return "#"+this.root_id+" ."+id;
    }
    return "#"+id;
}

AfterqueryObj.prototype.err = function(s) {
    $(this.elid("vizlog")).append('\n' + s);
  };


AfterqueryObj.prototype.showstatus = function(s, s2) {
    $(this.elid('statustext')).html(s);
    $(this.elid('statussub')).text(s2 || '');
    if (s || s2) {
      AfterqueryObj.log('status message:', s, s2);
      $(this.elid('vizstatus')).show();
    } else {
      $(this.elid('vizstatus')).hide();
    }
  };

AfterqueryObj.log = function() {
    if(0) {
        var args = ["render.js:"]; 
        for(var i = 0; i < arguments.length; i++) { args.push(arguments[i]); }
        console.log.apply(console, args);
    }
}

AfterqueryObj.parseArgs = function(query) {
    var kvlist;
    if (query.join) {
      // user provided an array of 'key=value' strings
      kvlist = query;
    } else {
      // assume user provided a single string
      if (query[0] == '?' || query[0] == '#') {
        query = query.substr(1);
      }
      kvlist = query.split('&');
    }
    var out = {};
    var outlist = [];
    for (var i in kvlist) {
      var kv = kvlist[i].split('=');
      var key = decodeURIComponent(kv.shift());
      var value = decodeURIComponent(kv.join('='));
      out[key] = value;
      outlist.push([key, value]);
    }
    AfterqueryObj.log('query args:', out);
    AfterqueryObj.log('query arglist:', outlist);
    return {
      get: function(key) { return out[key]; },
      all: outlist
    };
  };



AfterqueryObj.looksLikeUrl = function(s) {
    var IS_URL_RE = RegExp('^(http|https)://');
    var url, label;
    var pos = (s || '').lastIndexOf('|');
    if (pos >= 0) {
      url = s.substr(0, pos);
      label = s.substr(pos + 1);
    } else {
      url = s;
      label = s;
    }
    if (IS_URL_RE.exec(s)) {
      return [url, label];
    } else {
      return;
    }
  };


AfterqueryObj.htmlEscape = function(s) {
    if (s == undefined) {
      return s;
    }
    return s.replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/\n/g, '<br>\n');
  };


AfterqueryObj.dataToGvizTable = function(grid, options) {
    if (!options) options = {};
    var is_html = options.allowHtml;
    var headers = grid.headers, data = grid.data, types = grid.types;
    var dheaders = [];
    for (var i in headers) {
      dheaders.push({
        id: headers[i],
        label: headers[i],
        type: (types[i] != AfterqueryObj.T_BOOL || !options.bool_to_num) ? types[i] : AfterqueryObj.T_NUM
      });
    }
    var ddata = [];
    for (var rowi in data) {
      var row = [];
      for (var coli in data[rowi]) {
        var cell = data[rowi][coli];
        if (is_html && types[coli] === AfterqueryObj.T_STRING) {
          cell = cell.toString();
          var urlresult = AfterqueryObj.looksLikeUrl(cell);
          if (urlresult) {
            cell = '<a href="' + encodeURI(urlresult[0]) + '">' +
                AfterqueryObj.htmlEscape(urlresult[1]) + '</a>';
          } else {
            cell = AfterqueryObj.htmlEscape(cell);
          }
        }
        var col = { v: cell };
        if (options.show_only_lastseg && col.v && col.v.split) {
          var lastseg = col.v.split('|').pop();
          if (lastseg != col.v) {
            col.f = lastseg;
          }
        }
        row.push(col);
      }
      ddata.push({c: row});
    }
    var datatable = new google.visualization.DataTable({
      cols: dheaders,
      rows: ddata
    });
    if (options.intensify) {
      var minval, maxval;
      var rowmin = [], rowmax = [];
      var colmin = [], colmax = [];
      for (var coli in grid.types) {
        if (grid.types[coli] !== AfterqueryObj.T_NUM) continue;
        for (var rowi in grid.data) {
          var cell = grid.data[rowi][coli];
          if (cell < (minval || 0)) minval = cell;
          if (cell > (maxval || 0)) maxval = cell;
          if (cell < (rowmin[rowi] || 0)) rowmin[rowi] = cell;
          if (cell > (rowmax[rowi] || 0)) rowmax[rowi] = cell;
          if (cell < (colmin[coli] || 0)) colmin[coli] = cell;
          if (cell > (colmax[coli] || 0)) colmax[coli] = cell;
        }
      }

      for (var coli in grid.types) {
        if (grid.types[coli] == AfterqueryObj.T_NUM) {
          var formatter = new google.visualization.ColorFormat();
          var mn, mx;
          if (options.intensify == 'xy') {
            mn = minval;
            mx = maxval;
          } else if (options.intensify == 'y') {
            AfterqueryObj.log(colmin, colmax);
            mn = colmin[coli];
            mx = colmax[coli];
          } else if (options.intensify == 'x') {
            throw new Error('sorry, intensify=x not supported yet');
          } else {
            throw new Error("unknown intensify= mode '" +
                            options.intensify + "'");
          }
          AfterqueryObj.log('coli=' + coli + ' mn=' + mn + ' mx=' + mx);
          formatter.addGradientRange(mn - 1, 0, null, '#f88', '#fff');
          formatter.addGradientRange(0, mx + 1, null, '#fff', '#88f');
          formatter.format(datatable, parseInt(coli));
        }
      }
    }
    return datatable;
  };



AfterqueryObj.T_NUM = 'number';
AfterqueryObj.T_DATE = 'date';
AfterqueryObj.T_DATETIME = 'datetime';
AfterqueryObj.T_BOOL = 'boolean';
AfterqueryObj.T_STRING = 'string';


AfterqueryObj.guessTypes = function(data) {
    var CANT_NUM = 1;
    var CANT_BOOL = 2;
    var CANT_DATE = 4;
    var CANT_DATETIME = 8;
    var impossible = [];
    for (var rowi in data) {
      var row = data[rowi];
      for (var coli in row) {
        impossible[coli] |= 0;
        var cell = row[coli];
        if (cell == '' || cell == null) continue;
        var d = AfterqueryObj.myParseDate(cell);
        if (isNaN(d)) {
          impossible[coli] |= CANT_DATE | CANT_DATETIME;
        } else if (d.getHours() || d.getMinutes() || d.getSeconds()) {
          impossible[coli] |= CANT_DATE; // has time, so isn't a pure date
        }
        var f = cell * 1;
        if (isNaN(f)) impossible[coli] |= CANT_NUM;
        if (!(cell == 0 || cell == 1 ||
              cell == 'true' || cell == 'false' ||
              cell == true || cell == false ||
              cell == 'True' || cell == 'False')) impossible[coli] |= CANT_BOOL;
      }
    }
    AfterqueryObj.log('guessTypes impossibility list:', impossible);
    var types = [];
    for (var coli in impossible) {
      var imp = impossible[coli];
      if (!(imp & CANT_BOOL)) {
        types[coli] = AfterqueryObj.T_BOOL;
      } else if (!(imp & CANT_DATE)) {
        types[coli] = AfterqueryObj.T_DATE;
      } else if (!(imp & CANT_DATETIME)) {
        types[coli] = AfterqueryObj.T_DATETIME;
      } else if (!(imp & CANT_NUM)) {
        types[coli] = AfterqueryObj.T_NUM;
      } else {
        types[coli] = AfterqueryObj.T_STRING;
      }
    }
    return types;
  };


AfterqueryObj.myParseDate = function(s) {
    // We want to support various different date formats that people
    // tend to use as strings, with and without time of day,
    // including yyyy-mm-dd hh:mm:ss.mmm, yyyy/mm/dd, mm/dd/yyyy hh:mm PST, etc.
    // This gets a little hairy because so many things are optional.
    // We could try to support two-digit years or dd-mm-yyyy formats, but
    // those create parser ambiguities, so let's avoid it.
    var DATE_RE1 = RegExp(
        '^(\\d{1,4})[-/](\\d{1,2})(?:[-/](\\d{1,4})' +
        '(?:[T\\s](\\d{1,2}):(\\d\\d)(?::(\\d\\d)(?:\\.(\\d+))?)?)?)?' +
        '(?:Z| \\w\\w\\w)?$');
    // Some people (gviz, for example) provide "json" files where dates
    // look like javascript Date() object declarations, eg.
    // Date(2014,0,1,2,3,4)
    var DATE_RE2 = /^Date\(([\d,]+)\)$/;
    if (s == null) return s;
    if (s && s.getDate) return s;

    // Try gviz-style Date(...) format first
    var g = DATE_RE2.exec(s);
    if (g) {
      g = (',' + g[1]).split(',');
      if (g.length >= 3) {
        g[2]++;  // date objects start at month=0, for some reason
      }
    }

    // If that didn't match, try string-style date format
    if (!g || g.length > 8) g = DATE_RE1.exec(s);
    if (g && (g[3] > 1000 || g[1] > 1000)) {
      if (g[3] > 1000) {
        // mm-dd-yyyy
        var yyyy = g[3], mm = g[1], dd = g[2];
      } else {
        // yyyy-mm-dd
        var yyyy = g[1], mm = g[2], dd = g[3];
      }
      if (g[7]) {
        // parse milliseconds
        while (g[7].length < 3) g[7] = g[7] + '0';
        for (var i = g[7].length; i > 3; i--) g[7] /= 10.0;
        g[7] = Math.round(g[7], 0);
      }
      return new Date(Date.UTC(yyyy, mm - 1, dd || 1,
                      g[4] || 0, g[5] || 0, g[6] || 0,
                      g[7] || 0));
    }
    return NaN;
  };


AfterqueryObj.zpad = function(n, width) {
    var s = '' + n;
    while (s.length < width) s = '0' + s;
    return s;
  };


AfterqueryObj.dateToStr = function(d) {
    if (!d) return '';
    return (d.getFullYear() + '-' +
            AfterqueryObj.zpad(d.getMonth() + 1, 2) + '-' +
            AfterqueryObj.zpad(d.getDate(), 2));
  };


AfterqueryObj.dateTimeToStr = function(d) {
    if (!d) return '';
    var msec = d.getMilliseconds();
    return (AfterqueryObj.dateToStr(d) + ' ' +
            AfterqueryObj.zpad(d.getHours(), 2) + ':' +
            AfterqueryObj.zpad(d.getMinutes(), 2) + ':' +
            AfterqueryObj.zpad(d.getSeconds(), 2) +
            (msec ? ('.' + AfterqueryObj.zpad(msec, 3)) : ''));
  };


AfterqueryObj.prototype.convertTypes = function(data, types) {
    for (var coli in types) {
      var type = types[coli];
      if (type === AfterqueryObj.T_DATE || type === AfterqueryObj.T_DATETIME) {
        for (var rowi in data) {
          data[rowi][coli] = AfterqueryObj.myParseDate(data[rowi][coli]);
        }
      } else if (type === AfterqueryObj.T_NUM || type === AfterqueryObj.T_BOOL) {
        for (var rowi in data) {
          var v = data[rowi][coli];
          if (v != null && v != '') {
            data[rowi][coli] = v * 1;
          }
        }
      }
    }
  };


AfterqueryObj.prototype.colNameToColNum = function(grid, colname) {
    var keycol = (colname == '*') ? 0 : grid.headers.indexOf(colname);
    if (keycol < 0) {
      throw new Error('unknown column name "' + colname + '"');
    }
    return keycol;
  };


AfterqueryObj.prototype.FUNC_RE = /^(\w+)\((.*)\)$/;

AfterqueryObj.prototype.keyToColNum = function(grid, key) {
    var g = this.FUNC_RE.exec(key);
    if (g) {
      return this.colNameToColNum(grid, g[2]);
    } else {
      return this.colNameToColNum(grid, key);
    }
  };


AfterqueryObj.prototype._groupByLoop = function(ingrid, keys, initval, addcols_func, putvalues_func) {
    var outgrid = {headers: [], data: [], types: []};
    var keycols = [];
    for (var keyi in keys) {
      var colnum = this.keyToColNum(ingrid, keys[keyi]);
      keycols.push(colnum);
      outgrid.headers.push(ingrid.headers[colnum]);
      outgrid.types.push(ingrid.types[colnum]);
    }

    addcols_func(outgrid);

    var out = {};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = [];
      for (var kcoli in keycols) {
        key.push(row[keycols[kcoli]]);
      }
      var orow = out[key];
      if (!orow) {
        orow = [];
        for (var keyi in keys) {
          orow[keyi] = row[keycols[keyi]];
        }
        for (var i = keys.length; i < outgrid.headers.length; i++) {
          orow[i] = initval;
        }
        out[key] = orow;
        // deliberately preserve sequencing as much as possible.  The first
        // time we see a given key is when we add it to the outgrid.
        outgrid.data.push(orow);
      }
      putvalues_func(outgrid, key, orow, row);
    }
    return outgrid;
  };

AfterqueryObj.prototype.agg_funcs = {
    first: function(l) {
      return l[0];
    },

    last: function(l) {
      return l.slice(l.length - 1)[0];
    },

    only: function(l) {
      if (l.length == 1) {
        return l[0];
      } else if (l.length < 1) {
        return null;
      } else {
        throw new Error('cell has more than one value: only(' + l + ')');
      }
    },

    min: function(l) {
      var out = null;
      for (var i in l) {
        if (out == null || l[i] < out) {
          out = l[i];
        }
      }
      return out;
    },

    max: function(l) {
      var out = null;
      for (var i in l) {
        if (out == null || l[i] > out) {
          out = l[i];
        }
      }
      return out;
    },

    cat: function(l) {
      return l.join(' ');
    },

    count: function(l) {
      return l.length;
    },

    count_nz: function(l) {
      var acc = 0;
      for (var i in l) {
        if (l[i] != null && l[i] != 0) {
          acc++;
        }
      }
      return acc;
    },

    count_distinct: function(l) {
      var a = {};
      for (var i in l) {
        a[l[i]] = 1;
      }
      var acc = 0;
      for (var i in a) {
        acc += 1;
      }
      return acc;
    },

    sum: function(l) {
      var acc;
      if (l.length) acc = 0;
      for (var i in l) {
        acc += parseFloat(l[i]) || 0;
      }
      return acc;
    },

    avg: function(l) {
      return AfterqueryObj.prototype.agg_funcs.sum(l) / AfterqueryObj.prototype.agg_funcs.count_nz(l);
    },

    // also works for non-numeric values, as long as they're sortable
    median: function(l) {
      var comparator = function(a, b) {
        a = a || '0'; // ensure consistent ordering given NaN and undefined
        b = b || '0';
        if (a < b) {
          return -1;
        } else if (a > b) {
          return 1;
        } else {
          return 0;
        }
      };
      if (l.length > 0) {
        l.sort(comparator);
        return l[parseInt(l.length / 2)];
      } else {
        return null;
      }
    },

    stddev: function(l) {
      var avg = AfterqueryObj.prototype.agg_funcs.avg(l);
      var sumsq = 0.0;
      for (var i in l) {
        var d = parseFloat(l[i]) - avg;
        if (d) sumsq += d * d;
      }
      return Math.sqrt(sumsq);
    },
    color: function(l,aqo) {
      for (var i in l) {
        var v = l[i];
        if (!(v in aqo.colormap)) {
          aqo.colormap[v] = ++aqo.next_color;
        }
        return aqo.colormap[v];
      }
    }
  };
AfterqueryObj.prototype.agg_funcs.count.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.count_nz.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.count_distinct.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.sum.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.avg.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.stddev.return_type = AfterqueryObj.T_NUM;
AfterqueryObj.prototype.agg_funcs.cat.return_type = AfterqueryObj.T_STRING;
AfterqueryObj.prototype.agg_funcs.color.return_type = AfterqueryObj.T_NUM;


AfterqueryObj.prototype.groupBy = function(ingrid, keys, values) {
    // add one value column for every column listed in values.
    var that = this;
    var valuecols = [];
    var valuefuncs = [];
    var addcols_func = function(outgrid) {
      for (var valuei in values) {
        var g = that.FUNC_RE.exec(values[valuei]);
        var field, func;
        if (g) {
          func = that.agg_funcs[g[1]];
          if (!func) {
            throw new Error('unknown aggregation function "' + g[1] + '"');
          }
          field = g[2];
        } else {
          func = null;
          field = values[valuei];
        }
        var colnum = that.keyToColNum(ingrid, field);
        if (!func) {
          if (ingrid.types[colnum] === AfterqueryObj.T_NUM ||
              ingrid.types[colnum] === AfterqueryObj.T_BOOL) {
            func = that.agg_funcs.sum;
          } else {
            func = that.agg_funcs.count;
          }
        }
        valuecols.push(colnum);
        valuefuncs.push(func);
        if (g) {
          outgrid.headers.push(field == '*' ? '_count' : g[1] + ingrid.headers[colnum]);
        } else {
          outgrid.headers.push(field == '*' ? '_count' : ingrid.headers[colnum]);
        }
        outgrid.types.push(func.return_type || ingrid.types[colnum]);
      }
    };

    // by default, we do a count(*) operation for non-numeric value
    // columns, and sum(*) otherwise.
    var putvalues_func = function(outgrid, key, orow, row) {
      for (var valuei in values) {
        var incoli = valuecols[valuei];
        var outcoli = key.length + parseInt(valuei);
        var cell = row[incoli];
        if (!orow[outcoli]) orow[outcoli] = [];
        if (cell != null) {
          orow[outcoli].push(cell);
        }
      }
    };

    var outgrid = this._groupByLoop(ingrid, keys, 0,
                               addcols_func, putvalues_func);

    for (var rowi in outgrid.data) {
      var row = outgrid.data[rowi];
      for (var valuei in values) {
        var outcoli = keys.length + parseInt(valuei);
        var func = valuefuncs[valuei];
        row[outcoli] = func(row[outcoli], this);
      }
    }

    return outgrid;
  };


AfterqueryObj.prototype.pivotBy = function(ingrid, rowkeys, colkeys, valkeys) {
    // We generate a list of value columns based on all the unique combinations
    // of (values in colkeys)*(column names in valkeys)
    var that = this;
    var valuecols = {};
    var colkey_outcols = {};
    var colkey_incols = [];
    for (var coli in colkeys) {
      colkey_incols.push(this.keyToColNum(ingrid, colkeys[coli]));
    }
    var addcols_func = function(outgrid) {
      for (var rowi in ingrid.data) {
        var row = ingrid.data[rowi];
        var colkey = [];
        for (var coli in colkey_incols) {
          var colnum = colkey_incols[coli];
          colkey.push(that.stringifiedCol(row[colnum], ingrid.types[colnum]));
        }
        for (var coli in valkeys) {
          var xcolkey = colkey.concat([valkeys[coli]]);
          if (!(xcolkey in colkey_outcols)) {
            // if there's only one valkey (the common case), don't include the
            // name of the old value column in the new column names; it's
            // just clutter.
            var name = valkeys.length > 1 ?
                xcolkey.join(' ') : colkey.join(' ');
            var colnum = rowkeys.length + colkeys.length + parseInt(coli);
            colkey_outcols[xcolkey] = outgrid.headers.length;
            valuecols[xcolkey] = colnum;
            outgrid.headers.push(name);
            outgrid.types.push(ingrid.types[colnum]);
          }
        }
      }
      AfterqueryObj.log('pivot colkey_outcols', colkey_outcols);
      AfterqueryObj.log('pivot valuecols:', valuecols);
    };

    // by the time pivotBy is called, we're guaranteed that there's only one
    // row with a given (rowkeys+colkeys) key, so there is only one value
    // for each value cell.  Thus we don't need to worry about count/sum here;
    // we just assign the values directly as we see them.
    var putvalues_func = function(outgrid, rowkey, orow, row) {
      var colkey = [];
      for (var coli in colkey_incols) {
        var colnum = colkey_incols[coli];
        colkey.push(that.stringifiedCol(row[colnum], ingrid.types[colnum]));
      }
      for (var coli in valkeys) {
        var xcolkey = colkey.concat([valkeys[coli]]);
        var outcolnum = colkey_outcols[xcolkey];
        var valuecol = valuecols[xcolkey];
        orow[outcolnum] = row[valuecol];
      }
    };

    return this._groupByLoop(ingrid, rowkeys, undefined,
                        addcols_func, putvalues_func);
  };


AfterqueryObj.prototype.stringifiedCol = function(value, typ) {
    if (typ === AfterqueryObj.T_DATE) {
      return AfterqueryObj.dateToStr(value) || '';
    } else if (typ === AfterqueryObj.T_DATETIME) {
      return AfterqueryObj.dateTimeToStr(value) || '';
    } else {
      return (value + '') || '(none)';
    }
  };


AfterqueryObj.prototype.stringifiedCols = function(row, types) {
    var out = [];
    for (var coli in types) {
      out.push(this.stringifiedCol(row[coli], types[coli]));
    }
    return out;
  };


AfterqueryObj.prototype.treeJoinKeys = function(ingrid, nkeys) {
    var outgrid = {
        headers: ['_tree'].concat(ingrid.headers.slice(nkeys)),
        types: [AfterqueryObj.T_STRING].concat(ingrid.types.slice(nkeys)),
        data: []
    };

    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = row.slice(0, nkeys);
      var newkey = this.stringifiedCols(row.slice(0, nkeys),
                                   ingrid.types.slice(0, nkeys)).join('|');
      outgrid.data.push([newkey].concat(row.slice(nkeys)));
    }
    return outgrid;
  };


AfterqueryObj.prototype.finishTree = function(ingrid, keys) {
    if (keys.length < 1) {
      keys = ['_tree'];
    }
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    var keycols = [];
    for (var keyi in keys) {
      keycols.push(this.keyToColNum(ingrid, keys[keyi]));
    }

    var seen = {};
    var needed = {};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = [];
      for (var keyi in keycols) {
        var keycol = keycols[keyi];
        key.push(row[keycol]);
      }
      seen[key] = 1;
      delete needed[key];
      outgrid.data.push(row);

      var treekey = key.pop().split('|');
      while (treekey.length > 0) {
        treekey.pop();
        var pkey = key.concat([treekey.join('|')]);
        if (pkey in needed || pkey in seen) break;
        needed[pkey] = [treekey.slice(), row];
      }
    }

    var treecol = keycols.pop();
    for (var needkey in needed) {
      var treekey = needed[needkey][0];
      var inrow = needed[needkey][1];
      var outrow = [];
      for (var keycoli in keycols) {
        var keycol = keycols[keycoli];
        outrow[keycol] = inrow[keycol];
      }
      outrow[treecol] = treekey.join('|');
      outgrid.data.push(outrow);
    }

    return outgrid;
  };


AfterqueryObj.prototype.invertTree = function(ingrid, key) {
    if (!key) {
      key = '_tree';
    }
    var keycol = this.keyToColNum(ingrid, key);
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var cell = row[keycol];
      var outrow = row.slice();
      outrow[keycol] = cell.split('|').reverse().join('|');
      outgrid.data.push(outrow);
    }
    return outgrid;
  };


AfterqueryObj.prototype.crackTree = function(ingrid, key) {
    if (!key) {
      key = '_tree';
    }
    var keycol = this.keyToColNum(ingrid, key);
    var outgrid = {
      headers:
        [].concat(ingrid.headers.slice(0, keycol),
                  ['_id', '_parent'],
                  ingrid.headers.slice(keycol + 1)),
      data: [],
      types:
        [].concat(ingrid.types.slice(0, keycol),
                  [AfterqueryObj.T_STRING, AfterqueryObj.T_STRING],
                  ingrid.types.slice(keycol + 1))
    };

    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var key = row[keycol];
      var pkey;
      if (!key) {
        key = 'ALL';
        pkey = '';
      } else {
        var keylist = key.split('|');
        keylist.pop();
        pkey = keylist.join('|');
        if (!pkey) {
          pkey = 'ALL';
        }
      }
      outgrid.data.push([].concat(row.slice(0, keycol),
                                  [key, pkey],
                                  row.slice(keycol + 1)));
   }
    return outgrid;
  };


AfterqueryObj.prototype.splitNoEmpty = function(s, splitter) {
    if (!s) return [];
    return s.split(splitter);
  };


AfterqueryObj.prototype.keysOtherThan = function(grid, keys) {
    var out = [];
    var keynames = [];
    for (var keyi in keys) {
      // this converts func(x) notation to just 'x'
      keynames.push(grid.headers[this.keyToColNum(grid, keys[keyi])]);
    }
    for (var coli in grid.headers) {
      if (keynames.indexOf(grid.headers[coli]) < 0) {
        out.push(grid.headers[coli]);
      }
    }
    return out;
  };


AfterqueryObj.prototype.doGroupBy = function(grid, argval) {
    AfterqueryObj.log('groupBy:', argval);
    var parts = argval.split(';', 2);
    var keys = this.splitNoEmpty(parts[0], ',');
    var values;
    if (parts.length >= 2) {
      // if there's a ';' separator, the names after it are the desired
      // value columns (and that list may be empty).
      var tmpvalues = this.splitNoEmpty(parts[1], ',');
      values = [];
      for (var tmpi in tmpvalues) {
        var tmpval = tmpvalues[tmpi];
        if (tmpval == '*') {
          values = values.concat(this.keysOtherThan(grid, keys.concat(values)));
        } else {
          values.push(tmpval);
        }
      }
    } else {
      // if there is no ';' at all, the default is to just pull in all the
      // remaining non-key columns as values.
      values = this.keysOtherThan(grid, keys);
    }
    AfterqueryObj.log('grouping by', keys, values);
    grid = this.groupBy(grid, keys, values);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doTreeGroupBy = function(grid, argval) {
    AfterqueryObj.log('treeGroupBy:', argval);
    var parts = argval.split(';', 2);
    var keys = this.splitNoEmpty(parts[0], ',');
    var values;
    if (parts.length >= 2) {
      // if there's a ';' separator, the names after it are the desired
      // value columns (and that list may be empty).
      values = this.splitNoEmpty(parts[1], ',');
    } else {
      // if there is no ';' at all, the default is to just pull in all the
      // remaining non-key columns as values.
      values = this.keysOtherThan(grid, keys);
    }
    AfterqueryObj.log('treegrouping by', keys, values);
    grid = this.groupBy(grid, keys, values);
    grid = this.treeJoinKeys(grid, keys.length);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doFinishTree = function(grid, argval) {
    AfterqueryObj.log('finishTree:', argval);
    var keys = this.splitNoEmpty(argval, ',');
    AfterqueryObj.log('finishtree with keys', keys);
    grid = this.finishTree(grid, keys);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doInvertTree = function(grid, argval) {
    AfterqueryObj.log('invertTree:', argval);
    var keys = this.splitNoEmpty(argval, ',');
    AfterqueryObj.log('invertTree with key', keys[0]);
    grid = this.invertTree(grid, keys[0]);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doCrackTree = function(grid, argval) {
    AfterqueryObj.log('crackTree:', argval);
    var keys = this.splitNoEmpty(argval, ',');
    AfterqueryObj.log('cracktree with key', keys[0]);
    grid = this.crackTree(grid, keys[0]);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doPivotBy = function(grid, argval) {
    AfterqueryObj.log('pivotBy:', argval);

    // the parts are rowkeys;colkeys;values
    var parts = argval.split(';', 3);
    var rowkeys = this.splitNoEmpty(parts[0], ',');
    var colkeys = this.splitNoEmpty(parts[1], ',');
    var values;
    if (parts.length >= 3) {
      // if there's a second ';' separator, the names after it are the desired
      // value columns.
      values = this.splitNoEmpty(parts[2], ',');
    } else {
      // if there is no second ';' at all, the default is to just pull
      // in all the remaining non-key columns as values.
      values = this.keysOtherThan(grid, rowkeys.concat(colkeys));
    }

    // first group by the rowkeys+colkeys, so there is only one row for each
    // unique rowkeys+colkeys combination.
    grid = this.groupBy(grid, rowkeys.concat(colkeys), values);
    AfterqueryObj.log('tmpgrid:', grid);

    // now actually do the pivot.
    grid = this.pivotBy(grid, rowkeys, colkeys, values);

    return grid;
  };


AfterqueryObj.prototype.filterBy = function(ingrid, key, op, values) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    var keycol = this.keyToColNum(ingrid, key);
    var wantvals = [];
    for (var valuei in values) {
      if (ingrid.types[keycol] === AfterqueryObj.T_NUM) {
        wantvals.push(parseFloat(values[valuei]));
      } else if (ingrid.types[keycol] === AfterqueryObj.T_DATE ||
                 ingrid.types[keycol] === AfterqueryObj.T_DATETIME) {
        wantvals.push(AfterqueryObj.dateTimeToStr(AfterqueryObj.myParseDate(values[valuei])));
      } else {
        wantvals.push(values[valuei]);
      }
    }

    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var cell = row[keycol];
      if (cell == undefined) {
        cell = null;
      }
      var keytype = ingrid.types[keycol];
      if (keytype == AfterqueryObj.T_DATE || keytype == AfterqueryObj.T_DATETIME) {
        cell = AfterqueryObj.dateTimeToStr(cell);
      }
      var found = 0;
      for (var valuei in wantvals) {
        if (op == '=' && cell == wantvals[valuei]) {
          found = 1;
        } else if (op == '==' && cell == wantvals[valuei]) {
          found = 1;
        } else if (op == '>=' && cell >= wantvals[valuei]) {
          found = 1;
        } else if (op == '<=' && cell <= wantvals[valuei]) {
          found = 1;
        } else if (op == '>' && cell > wantvals[valuei]) {
          found = 1;
        } else if (op == '<' && cell < wantvals[valuei]) {
          found = 1;
        } else if (op == '!=' && cell != wantvals[valuei]) {
          found = 1;
        } else if (op == '<>' && cell != wantvals[valuei]) {
          found = 1;
        }
        if (found) break;
      }
      if (found) outgrid.data.push(row);
    }
    return outgrid;
  };


AfterqueryObj.prototype.trySplitOne = function(argval, splitstr) {
    var pos = argval.indexOf(splitstr);
    if (pos >= 0) {
      return [argval.substr(0, pos).trim(),
              argval.substr(pos + splitstr.length).trim()];
    } else {
      return;
    }
  };


AfterqueryObj.prototype.doFilterBy = function(grid, argval) {
    AfterqueryObj.log('filterBy:', argval);
    var ops = ['>=', '<=', '==', '!=', '<>', '>', '<', '='];
    var parts;
    for (var opi in ops) {
      var op = ops[opi];
      if ((parts = this.trySplitOne(argval, op))) {
        var matches = parts[1].split(',');
        AfterqueryObj.log('filterBy parsed:', parts[0], op, matches);
        grid = this.filterBy(grid, parts[0], op, matches);
        AfterqueryObj.log('grid:', grid);
        return grid;
      }
    }
    throw new Error('unknown filter operation in "' + argval + '"');
    return grid;
  };


AfterqueryObj.prototype.queryBy = function(ingrid, words) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi in ingrid.data) {
      var row = ingrid.data[rowi];
      var found = 0, skipped = 0;
      for (var wordi in words) {
        var word = words[wordi];
        if (word[0] == '!' || word[0] == '-') {
          found = 1;
        }
        for (var coli in row) {
          var cell = row[coli];
          if (cell != null && cell.toString().indexOf(word) >= 0) {
            found = 1;
            break;
          } else if ((word[0] == '!' || word[0] == '-') &&
                     (cell != null &&
                      cell.toString().indexOf(word.substr(1)) >= 0)) {
            skipped = 1;
            break;
          }
        }
        if (found || skipped) break;
      }
      if (found && !skipped) {
        outgrid.data.push(row);
      }
    }
    return outgrid;
  };


AfterqueryObj.prototype.doQueryBy = function(grid, argval) {
    AfterqueryObj.log('queryBy:', argval);
    grid = this.queryBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.deltaBy = function(ingrid, keys) {
    var outgrid = {headers: ingrid.headers, data: [], types: ingrid.types};
    for (var rowi = 0; rowi < ingrid.data.length; rowi++) {
      var row = ingrid.data[rowi];
      outgrid.data.push(row);
    }

    var keycols = [];
    for (var keyi in keys) {
      var key = keys[keyi];
      keycols.push(this.keyToColNum(ingrid, key));
    }

    if (outgrid.data.length < 2) {
      return outgrid;
    }
    for (var keyi in keycols) {
      var keycol = keycols[keyi];

      var prev_val = undefined;
      for (var rowi = 1; rowi < outgrid.data.length; rowi++) {
        var row = outgrid.data[rowi];
        var val = row[keycol];
        if (val == undefined) {
          continue;
        } else if (outgrid.types[keycol] === AfterqueryObj.T_NUM) {
          if (prev_val != undefined) {
            if (val > prev_val) {
              var new_val = val - prev_val;
              outgrid.data[rowi][keycol] = new_val;
            } else if (val == prev_val) {
              outgrid.data[rowi][keycol] = undefined;
            }
          }
          prev_val = val;
        }
      }
    }

    return outgrid;
  };


AfterqueryObj.prototype.doDeltaBy = function(grid, argval) {
    AfterqueryObj.log('deltaBy:', argval);
    grid = this.deltaBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.unselectBy = function(ingrid, keys) {
    var outgrid = {headers: [], data: [], types: []};
    var keycols = {};
    for (var keyi in keys) {
      var key = keys[keyi];
      var col = this.keyToColNum(ingrid, key);
      keycols[col] = true;
    }

    for (var headi = 0; headi < ingrid.headers.length; headi++) {
      if (!(headi in keycols)) {
        outgrid.headers.push(ingrid.headers[headi]);
        outgrid.types.push(ingrid.types[headi]);
      }
    }
    for (var rowi = 0; rowi < ingrid.data.length; rowi++) {
      var row = ingrid.data[rowi];
      var newrow = [];
      for (var coli = 0; coli < row.length; coli++) {
        if (!(coli in keycols)) {
          newrow.push(row[coli]);
        }
      }
      outgrid.data.push(newrow);
    }

    return outgrid;
  };


AfterqueryObj.prototype.doUnselectBy = function(grid, argval) {
    AfterqueryObj.log('unselectBy:', argval);
    grid = this.unselectBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.orderBy = function(grid, keys) {
    var that = this;
    var keycols = [];
    for (var keyi in keys) {
      var key = keys[keyi];
      var invert = 1;
      if (key[0] == '-') {
        invert = -1;
        key = key.substr(1);
      }
      keycols.push([this.keyToColNum(grid, key), invert]);
    }
    AfterqueryObj.log('sort keycols', keycols);
    var comparator = function(a, b) {
      for (var keyi in keycols) {
        var keycol = keycols[keyi][0], invert = keycols[keyi][1];
        var av = a[keycol], bv = b[keycol];
        if (grid.types[keycol] === AfterqueryObj.T_NUM) {
          av = parseFloat(av);
          bv = parseFloat(bv);
        }
        av = av || '0'; // ensure consistent ordering given NaN and undefined
        bv = bv || '0';
        if (av < bv) {
          return -1 * invert;
        } else if (av > bv) {
          return 1 * invert;
        }
      }
      return 0;
    };
    var outdata = grid.data.concat();
    outdata.sort(comparator);
    return { headers: grid.headers, data: outdata, types: grid.types };
  };


AfterqueryObj.prototype.doOrderBy = function(grid, argval) {
    AfterqueryObj.log('orderBy:', argval);
    grid = this.orderBy(grid, argval.split(','));
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.extractRegexp = function(grid, colname, regexp) {
    var r = RegExp(regexp);
    var colnum = this.keyToColNum(grid, colname);
    var typ = grid.types[colnum];
    grid.types[colnum] = AfterqueryObj.T_STRING;
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      var match = r.exec(this.stringifiedCol(row[colnum], typ));
      if (match) {
        row[colnum] = match.slice(1).join('');
      } else {
        row[colnum] = '';
      }
    }
    return grid;
  };


AfterqueryObj.prototype.doExtractRegexp = function(grid, argval) {
    AfterqueryObj.log('extractRegexp:', argval);
    var parts = this.trySplitOne(argval, '=');
    var colname = parts[0], regexp = parts[1];
    if (regexp.indexOf('(') < 0) {
      throw new Error('extract_regexp should have at least one (regex group)');
    }
    grid = this.extractRegexp(grid, colname, regexp);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.quantize = function(grid, colname, quants) {
    var colnum = this.keyToColNum(grid, colname);
    if (quants.length == 0) {
      throw new Error('quantize needs a bin size or list of edges');
    } else if (quants.length == 1) {
      // they specified a bin size
      var binsize = quants[0] * 1;
      if (binsize <= 0) {
        throw new Error('quantize: bin size ' + binsize + ' must be > 0');
      }
      for (var rowi in grid.data) {
        var row = grid.data[rowi];
        var binnum = Math.floor(row[colnum] / binsize);
        row[colnum] = binnum * binsize;
      }
    } else {
      // they specified the actual bin edges
      for (var rowi in grid.data) {
        var row = grid.data[rowi];
        var val = row[colnum];
        var out = undefined;
        for (var quanti in quants) {
          if (val * 1 < quants[quanti] * 1) {
            if (quanti == 0) {
              out = '<' + quants[0];
              break;
            } else {
              out = quants[quanti - 1] + '-' + quants[quanti];
              break;
            }
          }
        }
        if (!out) out = quants[quants.length - 1] + '+';
        row[colnum] = out;
      }
    }
    return grid;
  };


AfterqueryObj.prototype.doQuantize = function(grid, argval) {
    AfterqueryObj.log('quantize:', argval);
    var parts = this.trySplitOne(argval, '=');
    var colname = parts[0], quants = parts[1].split(',');
    grid = this.quantize(grid, colname, quants);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };

AfterqueryObj.prototype.doRename = function(ingrid, argval) {
    AfterqueryObj.log('rename:', argval);
    var parts = this.trySplitOne(argval, '=');
    var src = parts[0];
    var dst = parts[1];
    var grid = {
      data: ingrid.data,
      types: ingrid.types,
      headers: [],
    };
    var i;
    var done = false;
    for(i = 0; i < ingrid.headers.length; i++) {
      var header = ingrid.headers[i];
      if((!done) && (header === src)) {
        header = dst;
        done = true;
      }
      grid.headers.push(header);
    }
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.yspread = function(grid) {
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      var total = 0;
      for (var coli in row) {
        if (grid.types[coli] == AfterqueryObj.T_NUM && row[coli]) {
          total += Math.abs(row[coli] * 1);
        }
      }
      if (!total) total = 1;
      for (var coli in row) {
        if (grid.types[coli] == AfterqueryObj.T_NUM && row[coli]) {
          row[coli] = row[coli] * 1 / total;
        }
      }
    }
    return grid;
  };


AfterqueryObj.prototype.doYSpread = function(grid, argval) {
    AfterqueryObj.log('yspread:', argval);
    if (argval) {
      throw new Error('yspread: no argument expected');
    }
    grid = this.yspread(grid);
    AfterqueryObj.log('grid:', grid);
    return grid;
  };


AfterqueryObj.prototype.doLimit = function(ingrid, limit) {
    limit = parseInt(limit);
    if (ingrid.data.length > limit) {
      return {
          headers: ingrid.headers,
          data: ingrid.data.slice(0, limit),
          types: ingrid.types
      };
    } else {
      return ingrid;
    }
  };


AfterqueryObj.prototype.limitDecimalPrecision = function(grid) {
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      for (var coli in row) {
        var cell = row[coli];
        if (cell === +cell) {
          row[coli] = parseFloat(cell.toPrecision(15));
        }
      }
    }
    return grid;
  };


AfterqueryObj.prototype.fillNullsWithZero = function(grid) {
    for (var rowi in grid.data) {
      var row = grid.data[rowi];
      for (var coli in row) {
        if (grid.types[coli] === AfterqueryObj.T_NUM && row[coli] == undefined) {
          row[coli] = 0;
        }
        if (grid.types[coli] === AfterqueryObj.T_STRING && row[coli] == undefined) {
          row[coli] = "_undefined_";
        }
      }
    }
    return grid;
  };


AfterqueryObj.prototype.isString = function(v) {
    return v.charAt !== undefined;
  };


AfterqueryObj.prototype.isArray = function(v) {
    return v.splice !== undefined;
  };


AfterqueryObj.prototype.isObject = function(v) {
    return typeof(v) === 'object';
  };


AfterqueryObj.prototype.isDate = function(v) {
    return v.getDate !== undefined;
  };


AfterqueryObj.prototype.isScalar = function(v) {
    return v == undefined || this.isString(v) || !this.isObject(v) || this.isDate(v);
  };


AfterqueryObj.prototype.check2d = function(rawdata) {
    if (!this.isArray(rawdata)) return false;
    for (var rowi = 0; rowi < rawdata.length && rowi < 5; rowi++) {
      var row = rawdata[rowi];
      if (!this.isArray(row)) return false;
      for (var coli = 0; coli < row.length; coli++) {
        var col = row[coli];
        if (!this.isScalar(col)) return false;
      }
    }
    return true;
  };


AfterqueryObj.prototype._copyObj = function(out, v) {
    for (var key in v) {
      out[key] = v[key];
    }
    return out;
  };


AfterqueryObj.prototype.copyObj = function(v) {
    return this._copyObj({}, v);
  };


AfterqueryObj.prototype.multiplyLists = function(out, l1, l2) {
    if (l1 === undefined) throw new Error('l1 undefined');
    if (l2 === undefined) throw new Error('l2 undefined');
    for (var l1i in l1) {
      var r1 = l1[l1i];
      for (var l2i in l2) {
        var r2 = l2[l2i];
        var o = this.copyObj(r1);
        this._copyObj(o, r2);
        out.push(o);
      }
    }
    return out;
  };


AfterqueryObj.prototype.flattenDict = function(headers, coldict, rowtmp, d) {
    var out = [];
    var lists = [];
    for (var key in d) {
      var value = d[key];
      if (this.isScalar(value)) {
        if (coldict[key] === undefined) {
          coldict[key] = headers.length;
          headers.push(key);
        }
        rowtmp[key] = value;
      } else if (this.isArray(value)) {
        lists.push(this.flattenList(headers, coldict, value));
      } else {
        lists.push(this.flattenDict(headers, coldict, rowtmp, value));
      }
    }

    // now multiply all the lists together
    var tmp1 = [{}];
    while (lists.length) {
      var tmp2 = [];
      this.multiplyLists(tmp2, tmp1, lists.shift());
      tmp1 = tmp2;
    }

    // this is apparently the "right" way to append a list to a list.
    Array.prototype.push.apply(out, tmp1);
    return out;
  };


AfterqueryObj.prototype.flattenList = function(headers, coldict, rows) {
    var out = [];
    for (var rowi in rows) {
      var row = rows[rowi];
      var rowtmp = {};
      var sublist = this.flattenDict(headers, coldict, rowtmp, row);
      this.multiplyLists(out, [rowtmp], sublist);
    }
    return out;
  };

AfterqueryObj.mergeGrids = function(a, b) {
    var b_colnum;
    var b_header;
    var a_colnum;
    var row;
    var i;

    var out = {
      headers: a.headers.slice(0),
      types: a.types.slice(0),
      data: a.data
    };

    var b_new_cols = [];

    // Stub in space for new data.
    var newrow = new Array(a.headers.length);
    for(i = 0; i < a.headers.length; i++) {
      switch(a.types[i]) {
      case AfterqueryObj.T_NUM: newrow[i] = NaN; break;
      case AfterqueryObj.T_DATE: newrow[i] = new Date(1970,1,1,0,0,0); break;
      case AfterqueryObj.T_DATETIME: newrow[i] = new Date(1970,1,1,0,0,0); break;
      case AfterqueryObj.T_BOOL: newrow[i] = NaN; break;
      case AfterqueryObj.T_STRING: newrow[i] = ''; break;
      default: newrow[i] = null;
      }
    }
    var newrows = new Array(b.data.length);
    for(row = 0; row < b.data.length; row++) {
      newrows[row] = newrow.slice(0);
    }

    // Copy over data where we have a header and type match.
    for(b_colnum = 0; b_colnum < b.headers.length; b_colnum++) {
      b_header =  b.headers[b_colnum];
      a_colnum = a.headers.indexOf(b_header);
      if(a_colnum < 0 || b.types[b_colnum] != a.types[a_colnum]) {
        // Can't merge. Next column.
        b_new_cols.push(b_colnum);
        continue;
      }
      for(row = 0; row < b.data.length; row++) {
        newrows[row][a_colnum] = b.data[row][b_colnum];
      }
    }

    // Everything left is new columns
    for(i = 0; i < b_new_cols.length; i++) {
      b_colnum = b_new_cols[i];
      out.headers.push(b.headers[b_colnum]);
      out.types.push(b.types[b_colnum]);
      for(row = 0; row < b.data.length; row++) {
        newrows[row].push( b.data[row][b_colnum]);
      }
    }


    out.data = out.data.concat(newrows);
    return out;
  }

AfterqueryObj.prototype.gridFromManyData = function(rawdata) {
    if(rawdata.length === 0) {
      return {headers: [], data: [], types: []};
    }

    var grid = this.gridFromData(rawdata[0]);
    var newgrid;
    var i;
    for(i = 1; i < rawdata.length; i++) {
      newgrid = this.gridFromData(rawdata[i]);
      grid = AfterqueryObj.mergeGrids(grid,newgrid);
    }
    return grid;
  }



AfterqueryObj.prototype.gridFromData = function(rawdata) {
    if (rawdata && rawdata.headers && rawdata.data && rawdata.types) {
      // already in grid format
      return rawdata;
    }

    var headers, data, types;

    var err;
    if (rawdata.errors && rawdata.errors.length) {
      err = rawdata.errors[0];
    } else if (rawdata.error) {
      err = rawdata.error;
    }
    if (err) {
      var msglist = [];
      if (err.message) msglist.push(err.message);
      if (err.detailed_message) msglist.push(err.detailed_message);
      throw new Error('Data provider returned an error: ' + msglist.join(': '));
    }

    if (rawdata.table) {
      // gviz format
      headers = [];
      for (var headeri in rawdata.table.cols) {
        headers.push(rawdata.table.cols[headeri].label ||
                     rawdata.table.cols[headeri].id);
      }
      data = [];
      for (var rowi in rawdata.table.rows) {
        var row = rawdata.table.rows[rowi];
        var orow = [];
        for (var coli in row.c) {
          var col = row.c[coli];
          var g;
          if (!col) {
            orow.push(null);
          } else {
            orow.push(col.v);
          }
        }
        data.push(orow);
      }
    } else if (rawdata.data && rawdata.cols) {
      // eqldata.com format
      headers = [];
      for (var coli in rawdata.cols) {
        var col = rawdata.cols[coli];
        headers.push(col.caption || col);
      }
      data = rawdata.data;
    } else if (this.check2d(rawdata)) {
      // simple [[cols...]...] (two-dimensional array) format, where
      // the first row is the headers.
      headers = rawdata[0];
      data = rawdata.slice(1);
    } else if (this.isArray(rawdata)) {
      // assume datacube format, which is a nested set of lists and dicts.
      // A dict contains a list of key (column name) and value (cell content)
      // pairs.  A list contains a set of lists or dicts, which corresponds
      // to another "dimension" of the cube.  To flatten it, we have to
      // replicate the row once for each element in the list.
      var coldict = {};
      headers = [];
      var rowdicts = this.flattenList(headers, coldict, rawdata);
      data = [];
      for (var rowi in rowdicts) {
        var rowdict = rowdicts[rowi];
        var row = [];
        for (var coli in headers) {
          row.push(rowdict[headers[coli]]);
        }
        data.push(row);
      }
    } else {
      throw new Error("don't know how to parse this json layout, sorry!");
    }
    types = AfterqueryObj.guessTypes(data);
    this.convertTypes(data, types);
    return {headers: headers, data: data, types: types};
  };


AfterqueryObj.prototype.enqueue = function(queue, stepname, func) {
    queue.push([stepname, func]);
  };


AfterqueryObj.prototype.runqueue = function(queue, ingrid, done, showstatus, wrap_each, after_each) {
    var step = function(i) {
      if (i < queue.length) {
        var el = queue[i];
        var text = el[0], func = el[1];
        if (showstatus) {
          showstatus('Running step ' + (+i + 1) + ' of ' +
                     queue.length + '...',
                     text);
        }
        setTimeout(function() {
          var start = Date.now();
          var wfunc = wrap_each ? wrap_each(func) : func;
          wfunc(ingrid, function(outgrid) {
            var end = Date.now();
            if (after_each) {
              after_each(outgrid, i + 1, queue.length, text, end - start);
            }
            ingrid = outgrid;
            step(i + 1);
          });
        }, 0);
      } else {
        if (showstatus) {
          showstatus('');
        }
        if (done) {
          done(ingrid);
        }
      }
    };
    step(0);
  }


AfterqueryObj.prototype.maybeSet = function(dict, key, value) {
    if (!(key in dict)) {
      dict[key] = value;
    }
  };


AfterqueryObj.prototype.addTransforms = function(queue, args) {
    var that = this;
    var trace = args.get('trace');
    var argi;

    // helper function for synchronous transformations (ie. ones that return
    // the output grid rather than calling a callback)
    var transform = function(f, arg) {
      that.enqueue(queue, args.all[argi][0] + '=' + args.all[argi][1],
              function(ingrid, done) {
        var outgrid = f(ingrid, arg);
        done(outgrid);
      });
    };

    for (var argi in args.all) {
      var argkey = args.all[argi][0], argval = args.all[argi][1];
      if (argkey == 'group') {
        transform(function(g,a){return that.doGroupBy(g,a);}, argval);
      } else if (argkey == 'treegroup') {
        transform(function(g,a){return that.doTreeGroupBy(g,a);}, argval);
      } else if (argkey == 'finishtree') {
        transform(function(g,a){return that.doFinishTree(g,a);}, argval);
      } else if (argkey == 'inverttree') {
        transform(function(g,a){return that.doInvertTree(g,a);}, argval);
      } else if (argkey == 'cracktree') {
        transform(function(g,a){return that.doCrackTree(g,a);}, argval);
      } else if (argkey == 'pivot') {
        transform(function(g,a){return that.doPivotBy(g,a);}, argval);
      } else if (argkey == 'filter') {
        transform(function(g,a){return that.doFilterBy(g,a);}, argval);
      } else if (argkey == 'q') {
        transform(function(g,a){return that.doQueryBy(g,a);}, argval);
      } else if (argkey == 'limit') {
        transform(function(g,a){return that.doLimit(g,a);}, argval);
      } else if (argkey == 'delta') {
        transform(function(g,a){return that.doDeltaBy(g,a);}, argval);
      } else if (argkey == 'unselect') {
        transform(function(g,a){return that.doUnselectBy(g,a);}, argval);
      } else if (argkey == 'order') {
        transform(function(g,a){return that.doOrderBy(g,a);}, argval);
      } else if (argkey == 'extract_regexp') {
        transform(function(g,a){return that.doExtractRegexp(g,a);}, argval);
      } else if (argkey == 'quantize') {
        transform(function(g,a){return that.doQuantize(g,a);}, argval);
      } else if (argkey == 'rename') {
        transform(function(g,a){return that.doRename(g,a);}, argval);
      } else if (argkey == 'yspread') {
        transform(function(g,a){return that.doYSpread(g,a);}, argval);
      }
    }
  };


AfterqueryObj.prototype.dyIndexFromX = function(dychart, x) {
    // TODO(apenwarr): consider a binary search
    for (var i = dychart.numRows() - 1; i >= 0; i--) {
      if (dychart.getValue(i, 0) <= x) {
        break;
      }
    }
    return i;
  };


AfterqueryObj.prototype.createTracesChart = function(grid, el, colsPerChart) {
    var that = this;
    var charts = [];
    var xlines = [];

    var n = (grid.headers.length - 1) / colsPerChart;
    $(el).css('overflow', 'scroll');
    var tab = $('<table class="dy-table" style="width:100%;"/>');
    $(el).append(tab);

    var zooming = false;
    for (var i = 0; i < n; i++) {
      var dyoptions = {
        connectSeparatedPoints: false,
        drawYAxis: true,
        drawXAxis: false,
        drawGrid: false,
        drawPoints: true,
        fillGraph: false,
        hideOverlayOnMouseOut: false,
        stepPlot: true,
        strokeWidth: 0,
        zoomCallback: function(x1, x2, yranges) {
          if (zooming) return;
          zooming = true;
          var range = [x1, x2];
          for (var charti = 0; charti < charts.length; charti++) {
            charts[charti].updateOptions({ dateWindow: range });
          }
          zooming = false;
        }
      };
      var coli = 1 + (i * colsPerChart);
      var row = $('<tr>');
      var labelcol = $('<td>').text(grid.headers[coli]);
      var col = $('<td style="width:100%">');
      $(tab).append(row);
      $(row).append(labelcol);
      $(row).append(col);
      var odd = (i % 2) ? 'odd' : 'even';
      var sub = $('<div class="dy-chart dy-chart-' + odd + '"/>')[0];
      $(sub).css('position', 'relative');
      $(col).append(sub);
      dyoptions.labels = [grid.headers[0], 'value'];

      var data = [];
      if (colsPerChart == 3) {
        // input data is x,val,min,max
        // output data must be x,[min,val,max]
        for (var rowi = 0; rowi < grid.data.length; rowi++) {
          var datacol = [grid.data[rowi][coli + 1],
                         grid.data[rowi][coli + 0],
                         grid.data[rowi][coli + 2]];
          if (rowi == 0 ||
              rowi == grid.data.length - 1 ||
              datacol[0] != null ||
              datacol[1] != null ||
              datacol[2] != null) {
            data.push([grid.data[rowi][0], datacol]);
          }
        }
      } else {
        for (var rowi = 0; rowi < grid.data.length; rowi++) {
          if (rowi == 0 ||
              rowi == grid.data.length - 1 ||
              grid.data[rowi][coli] != null) {
            data.push([grid.data[rowi][0], grid.data[rowi][coli]]);
          }
        }
      }

      if (i == n - 1) {
        $(sub).css('height', '140px');
        dyoptions.drawXAxis = true;
      } else {
        $(sub).css('height', '120px');
      }

      var xline = $('<div class="dy-line dy-xline" />')[0];
      var yline = $('<div class="dy-line dy-yline" />')[0];
      var inHighlighter = false;
      var declareCallback = function(data, xline, yline) {
        return function(graph, series, canvas, x, y, color, size, idx) {
          $(xline).css('left', x - 1 + 'px');
          $(yline).css('top', y - 1 + 'px');

          // also move the highlight in all other charts
          if (inHighlighter) return;
          inHighlighter = true;
          for (var charti = 0; charti < charts.length; charti++) {
            var otheridx = that.dyIndexFromX(charts[charti], data[idx][0]);
            charts[charti].setSelection(otheridx);
          }
          inHighlighter = false;
        };
      };
      dyoptions.drawHighlightPointCallback = declareCallback(data,
                                                             xline, yline);
      if (colsPerChart == 3) {
        dyoptions.customBars = true;
      } else {
        dyoptions.customBars = false;
      }

      var chart = new Dygraph(sub, data, dyoptions);
      $(sub).append(xline, yline);
      charts.push(chart);
    }

    return { draw: function(table, options) { } };
  };


AfterqueryObj.NaNToZeroFormatter = function(dt, col) {
  for(var row = 0; row<dt.getNumberOfRows(); row++) {
    if(isNaN(dt.getValue(row, col))) {
      dt.setValue(row, col, 0);
      dt.setFormattedValue(row, col, '');
    }
  }
}

AfterqueryObj.prototype.addRenderers = function(queue, args, more_options_in) {
    var that = this;
    var has_more_opts = more_options_in !== undefined;
    var more_options = more_options_in;
    var trace = args.get('trace');
    var chartops = args.get('chart');
    var t, datatable, resizeTimer;
    var options = {};
    var intensify = args.get('intensify');
    if (intensify == '') {
      intensify = 'xy';
    }
    var gridoptions = {
      intensify: intensify
    };
    var el = $(this.elid("vizchart"))[0];

    this.enqueue(queue, 'gentable', function(grid, done) {
      // Some charts react badly to missing values, so fill them in.
      grid = that.fillNullsWithZero(grid);
      if (chartops) {
        var chartbits = chartops.split(',');
        var charttype = chartbits.shift();
        for (var charti in chartbits) {
          var kv = that.trySplitOne(chartbits[charti], '=');
          options[kv[0]] = kv[1];
        }
        grid = that.limitDecimalPrecision(grid);

        // Scan and add all args not for afterquery to GViz options.
        that.scanGVizChartOptions(args, options);

        if (charttype == 'stackedarea' || charttype == 'stacked') {
          t = new google.visualization.AreaChart(el);
          options.isStacked = true;
          options.explorer = {};
        } else if (charttype == 'column') {
          t = new google.visualization.ColumnChart(el);
        } else if (charttype == 'bar') {
          t = new google.visualization.BarChart(el);
        } else if (charttype == 'line') {
          t = new google.visualization.LineChart(el);
        } else if (charttype == 'spark') {
          // sparkline chart: get rid of everything but the data series.
          // Looks best when small.
          options.hAxis = {};
          options.hAxis.baselineColor = 'none';
          options.hAxis.textPosition = 'none';
          options.hAxis.gridlines = {};
          options.hAxis.gridlines.color = 'none';
          options.vAxis = {};
          options.vAxis.baselineColor = 'none';
          options.vAxis.textPosition = 'none';
          options.vAxis.gridlines = {};
          options.vAxis.gridlines.color = 'none';
          options.theme = 'maximized';
          options.legend = {};
          options.legend.position = 'none';
          t = new google.visualization.LineChart(el);
        } else if (charttype == 'pie') {
          t = new google.visualization.PieChart(el);
        } else if (charttype == 'tree') {
          if (grid.headers[0] == '_tree') {
            grid = that.finishTree(grid, ['_tree']);
            grid = that.crackTree(grid, '_tree');
          }
          that.maybeSet(options, 'maxDepth', 3);
          that.maybeSet(options, 'maxPostDepth', 1);
          that.maybeSet(options, 'showScale', 1);
          t = new google.visualization.TreeMap(el);
        } else if (charttype == 'candle' || charttype == 'candlestick') {
          t = new google.visualization.CandlestickChart(el);
        } else if (charttype == 'timeline') {
          t = new google.visualization.AnnotatedTimeLine(el);
        } else if (charttype == 'dygraph' || charttype == 'dygraph+errors') {
          t = new Dygraph.GVizChart(el);
          that.maybeSet(options, 'showRoller', true);
          if (charttype == 'dygraph+errors') {
            options.errorBars = true;
          }
        } else if (charttype == 'traces' || charttype == 'traces+minmax') {
          t = that.createTracesChart(grid, el,
                                charttype == 'traces+minmax' ? 3 : 1);
        } else if (charttype == 'heatgrid') {
          t = new HeatGrid(el);
        } else {
          throw new Error('unknown chart type "' + charttype + '"');
        }
        gridoptions.show_only_lastseg = true;
        gridoptions.bool_to_num = true;
      } else {
        t = new google.visualization.Table(el);
        gridoptions.allowHtml = true;
        options.allowHtml = true;
      }

      if (charttype == 'heatgrid' ||
          charttype == 'traces' ||
          charttype == 'traces+minmax') {
        datatable = grid;
      } else {
        datatable = AfterqueryObj.dataToGvizTable(grid, gridoptions);

        var dateformat = new google.visualization.DateFormat({
          pattern: 'yyyy-MM-dd'
        });
        var datetimeformat = new google.visualization.DateFormat({
          pattern: 'yyyy-MM-dd HH:mm:ss'
        });
        for (var coli = 0; coli < grid.types.length; coli++) {
          if (grid.types[coli] === AfterqueryObj.T_DATE) {
            dateformat.format(datatable, coli);
          } else if (grid.types[coli] === AfterqueryObj.T_DATETIME) {
            datetimeformat.format(datatable, coli);
          } else if (grid.types[coli] === AfterqueryObj.T_NUM) {
            if(has_more_opts &&
              more_options.num_pattern !== undefined) {
              var numformat = new google.visualization.NumberFormat({
                pattern: more_options.num_pattern
              });
              numformat.format(datatable, coli);
              AfterqueryObj.NaNToZeroFormatter(datatable, coli);
            }
          }
        }
      }
      done(grid);
    });

    this.enqueue(queue, chartops ? 'chart=' + chartops : 'view',
            function(grid, done) {
      if (grid.data.length) {
        var doRender = function() {
          var wantwidth = trace ? $(el).innerWidth - 40 : $(el).innerWidth;
          $(el).width(wantwidth);
          if(!has_more_opts || !more_options.disable_height) {
            $(el).height(window.innerHeight);
            options.height = window.innerHeight;
          }
          t.draw(datatable, options);
          if(has_more_opts && more_options.select_handler !== undefined) {
            google.visualization.events.addListener(t, 'select', function(e) { more_options.select_handler(e,t, datatable); });
          }
        };
        doRender();
        $(window).resize(function() {
          clearTimeout(resizeTimer);
          resizeTimer = setTimeout(doRender, 200);
        });
      } else {
        el.innerHTML = 'Empty dataset.';
      }
      done(grid);
    });
  };

AfterqueryObj.prototype.scanGVizChartOptions = function(args, options) {
    // Parse args to be sent to GViz.
    var allArgs = args['all'];
    for (var i in allArgs) {
      var key = allArgs[i][0];
      // Ignore params sent for afterquery.
      if (key == 'trace'
          || key == 'url'
          || key == 'chart'
          || key == 'editlink'
          || key == 'intensify'
          || key == 'order'
          || key == 'group'
          || key == 'limit'
          || key == 'filter'
          || key == 'q'
          || key == 'pivot'
          || key == 'treegroup'
          || key == 'extract_regexp') {
        continue;
      }
      // Add params for GViz API into options object.
      this.addGVizChartOption(options, key, allArgs[i][1]);
    }
    AfterqueryObj.log('Options sent to GViz');
    AfterqueryObj.log(options);
  };

AfterqueryObj.prototype.addGVizChartOption = function(options, key, value) {
    if (key.indexOf('.') > -1) {
      var subObjects = key.split('.');
      if (!options[subObjects[0]]) {
        options[subObjects[0]] = {};
      }
      this.addGVizChartOption(options[subObjects[0]],
        key.substring(key.indexOf('.') + 1), value);
    } else {
      options[key] = value;
    }
  };

// Needs to be shared by all instances.
AfterqueryObj.vizstep = 0;

AfterqueryObj.prototype.finishQueue = function(queue, args, done) {
    var that = this;
    var trace = args.get('trace');
    if (trace) {
      var prevdata;
      var after_each = function(grid, stepi, nsteps, text, msec_time) {
        AfterqueryObj.vizstep++;
        $(that.elid('vizlog')).append('<div class="vizstep" id="step' + AfterqueryObj.vizstep + '">' +
                            '  <div class="text"></div>' +
                            '  <div class="grid"></div>' +
                            '</div>');
        $('#step' + AfterqueryObj.vizstep + ' .text').text('Step ' + stepi +
                                           ' (' + msec_time + 'ms):  ' +
                                           text);
        var viewel = $('#step' + AfterqueryObj.vizstep + ' .grid');
        if (prevdata != grid.data) {
          var t = new google.visualization.Table(viewel[0]);
          var datatable = AfterqueryObj.dataToGvizTable({
            headers: grid.headers,
            data: grid.data.slice(0, 1000),
            types: grid.types
          });
          t.draw(datatable);
          prevdata = grid.data;
        } else {
          viewel.text('(unchanged)');
        }
        if (stepi == nsteps) {
          $('.vizstep').show();
        }
      };
      this.runqueue(queue, null, done, function(s,s2){that.showstatus(s,s2);}, function(f){return that.wrap(f);}, after_each);
    } else {
      this.runqueue(queue, null, done, function(s,s2){that.showstatus(s,s2);}, function(f){return that.wrap(f);});
    }
  };


AfterqueryObj.prototype.gotError = function(state) {
    this.showstatus('');
    var msg = "<p>Unable to load any data files.</p>\n<table>";
    for(var i = 0; i < state.failure.length; i++) {
      msg += "<tr>" +
        "<td><a href='"+encodeURI(state.failure[i].url)+"'>" +
          encodeURI(state.failure[i].url)+"</a></td>" +
        "<td>"+ AfterqueryObj.htmlEscape(state.failure[i].status)+"</td>" +
        "</tr>\n";
    }
    if(state.failure.length == 0) {
      msg += "<tr><td>>No errors were logged.</td></tr>>\n";
    }
    msg += "</table>";

    $(this.elid('vizraw')).html(msg);
    var msg_txt = $(this.elid('vizraw')).text();
    throw new Error(msg_txt);
  };


AfterqueryObj.prototype.argsToArray = function(args) {
    // call Array's slice() function on an 'arguments' structure, which is
    // like an array but missing functions like slice().  The result is a
    // real Array object, which is more useful.
    return [].slice.apply(args);
  };


AfterqueryObj.prototype.wrap = function(func) {
    // pre_args is the arguments as passed at wrap() time
    var that = this;
    var pre_args = this.argsToArray(arguments).slice(1);
    var f = function() {
      try {
        // post_args is the arguments as passed when calling f()
        var post_args = that.argsToArray(arguments);
        return func.apply(null, pre_args.concat(post_args));
      } catch (e) {
        $(that.elid('vizchart')).hide();
        $(that.elid('vizstatus')).css('position', 'relative');
        if(that.rootid) {
          $("#" + that.rootid + ' .vizstep').show();
        } else {
          $('.vizstep').show();
        }
        that.err(e);
        that.err("<p><a href='/help'>here's the documentation</a>");
        throw e;
      }
    };
    return f;
  };


AfterqueryObj.prototype.urlMinusPath = function(url) {
    var URL_RE = RegExp('^((\\w+:)?(//[^/]*)?)');
    var g = URL_RE.exec(url);
    if (g && g[1]) {
      return g[1];
    } else {
      return url;
    }
  };


AfterqueryObj.prototype.checkUrlSafety = function(url) {
    if (/[<>"''"]/.exec(url)) {
      throw new Error('unsafe url detected. encoded=' + encodedURI(url));
    }
  };


AfterqueryObj.prototype.extendDataUrl = function(url) {
    // some services expect callback=, some expect jsonp=, so supply both
    var plus = 'callback=jsonp&jsonp=jsonp';
    var hostpart = this.urlMinusPath(url);
    var auth;
    if( (/\/\//.exec(url))) {
      // No "//"? Probably a local file, and this operation
      // is doomed.
      auth = this.localStorage[['auth', hostpart]];
    }
    if (auth) {
      plus += '&auth=' + encodeURIComponent(auth);
    }

    if (url.indexOf('?') >= 0) {
      return url + '&' + plus;
    } else {
      return url + '?' + plus;
    }
  };


AfterqueryObj.prototype.extractJsonFromJsonp = function(text, success_func) {
    var data = text.trim();
    var start = data.indexOf('jsonp(');
    if (start >= 0) {
      data = data.substr(start + 6, data.length - start - 6 - 2);
      data = data.trim();
    }

    // Drop spurious trailing comma.
    // Likely in programmatically generated data where a comma is
    // appended to every record.
    if(data[data.length-1] === ',') {
      data = data.slice(0,-1);
    }

    // Ensure there is a "[" "]" wrapper around the whole thing.
    // Likely in programmatically generated data where new data is
    // regularly appended.  Maintaining the framing "[" "]" is a 
    // nuisance, so it doesn't get done.
    if(data.charAt(0) !== '[') {
      data = "[" + data + "]";
    }

    data = JSON.parse(data);
    success_func(data);
  };


AfterqueryObj.prototype.getUrlData_xhr = function(state, success_func, error_func) {
    var that = this;
    jQuery.support.cors = true;
    jQuery.ajax(state.todo[0], {
      headers: { 'X-DataSource-Auth': 'a' },
      xhrFields: { withCredentials: true },
      dataType: 'text',
      success: function(text) {
        that.extractJsonFromJsonp(text, function(grid) {
        that.getUrlDataSuccess(grid, state, success_func, error_func);
          }
          );
        },
      error: function(jqXHR, textStatus, errorThrown) {
        AfterqueryObj.log("XHR failed:", state.todo[0], textStatus, errorThrown, "Trying JSON.");
        that.getUrlData_jsonp(state, success_func, error_func);
        }
      }
    );
  }


AfterqueryObj.prototype.getUrlData_jsonp = function(state, success_func, error_func) {
    var that = this;
    var url = state.todo[0];
    var iframe = document.createElement('iframe');
    iframe.style.display = 'none';

    iframe.onload = function() {
      var failfunc_called;
      var successfunc_called;
      var real_success_func = function(data) {
        AfterqueryObj.log('calling success_func');
        that.getUrlDataSuccess(data, state, success_func, error_func);
        successfunc_called = true;
      };

      // the default jsonp callback
      iframe.contentWindow.jsonp = real_success_func;

      // a callback the jsonp can execute if oauth2 authentication is needed
      iframe.contentWindow.tryOAuth2 = function(oauth2_url) {
        var hostpart = that.urlMinusPath(oauth2_url);
        var oauth_appends = {
          'https://accounts.google.com':
              'client_id=41470923181.apps.googleusercontent.com'
          // (If you register afterquery with any other API providers, add the
          //  app ids here.  app client_id fields are not secret in oauth2;
          //  there's a client_secret, but it's not needed in pure javascript.)
        };
        var plus = [oauth_appends[hostpart]];
        if (plus) {
          plus += '&response_type=token';
          plus += '&state=' +
              encodeURIComponent(
                  'url=' + encodeURIComponent(url) +
                  '&continue=' + encodeURIComponent(window.top.location));
          plus += '&redirect_uri=' +
              encodeURIComponent(window.location.origin + '/oauth2callback');
          var want_url;
          if (oauth2_url.indexOf('?') >= 0) {
            want_url = oauth2_url + '&' + plus;
          } else {
            want_url = oauth2_url + '?' + plus;
          }
          AfterqueryObj.log('oauth2 redirect:', want_url);
          that.checkUrlSafety(want_url);
          document.write('Click here to ' +
                         '<a target="_top" ' +
                         '  href="' + want_url +
                         '">authorize the data source</a>.');
        } else {
          AfterqueryObj.log('no oauth2 service known for host', hostpart);
          document.write("Data source requires authorization, but I don't " +
                         'know how to oauth2 authorize urls from <b>' +
                         encodeURI(hostpart) +
                         '</b> - sorry.');
        }
        // needing oauth2 is "success" in that we know what's going on
        successfunc_called = true;
      };

      // some services are hardcoded to use the gviz callback, so
      // supply that too
      iframe.contentWindow.google = {
        visualization: {
          Query: {
            setResponse: real_success_func
          }
        }
      };

      iframe.contentWindow.onerror = function(message, xurl, lineno) {
        that.err(message + ' url=' + xurl + ' line=' + lineno);
        if(!failfunc_called) {
          that.getUrlDataFailure("Error loading data; check javascript console for details. "+message+" url="+xurl+" line="+lineno, "", state, success_func, error_func);
          failfunc_called = true;
        }
      };

      iframe.contentWindow.onpostscript = function() {
        if (successfunc_called) {
          AfterqueryObj.log('json load was successful.');
        } else {
          if(!failfunc_called) {
            that.getUrlDataFailure("Error loading data; check javascript console for details.", "", state, success_func, error_func);
            failfunc_called = true;
          }
        }
      };

      iframe.contentWindow.jsonp_url = url;

      //TODO(apenwarr): change the domain/origin attribute of the iframe.
      //  That way the script won't be able to affect us, no matter how badly
      //  behaved it might be.  That's important so they can't access our
      //  localStorage, set cookies, etc.  We can use the new html5 postMessage
      //  feature to safely send json data from the iframe back to us.
      // ...but for the moment we have to trust the data provider.
      //TODO(apenwarr): at that time, make this script.async=1
      //  ...and run postscript from there.

      var script = iframe.contentDocument.createElement('script');
      script.async = false;
      script.src = url;
      iframe.contentDocument.body.appendChild(script);

      var postscript = iframe.contentDocument.createElement('script');
      postscript.async = false;
      postscript.src = 'postscript.js';
      iframe.contentDocument.body.appendChild(postscript);
    };
    document.body.appendChild(iframe);
  };

AfterqueryObj.prototype.getUrlDataSuccess = function(data, state, success_func, error_func) {
    var that = this;
    var url = state.todo.shift();
    state.success.push(url);
    state.rawdata.push(data);
    setTimeout(function(){ that.getUrlData(state, success_func, error_func); }, 0);
  }

AfterqueryObj.prototype.getUrlDataFailure = function(textStatus, errorThrown, state, success_func, error_func) {
    var that = this;
    var url = state.todo.shift();
    state.failure.push({url:url, status: textStatus+" "+errorThrown});
    setTimeout(function(){ that.getUrlData(state, success_func, error_func); }, 0);
  }


AfterqueryObj.prototype.getUrlData = function(state, success_func, error_func) {
    if(state.todo.length === 0) {
      // All URLs attempted
      if(state.rawdata.length > 0) {
        // At least one success is considered success.
        success_func(state.rawdata);
      } else {
        // Failure
        error_func(state);
      }
      return;
    }

    var url = state.todo[0];
    this.showstatus('Loading <a href="' + encodeURI(url) + '">data</a>...');

    var that = this;
    AfterqueryObj.log('fetching data url:', url);
    this.getUrlData_xhr(state, success_func, error_func);
  };


AfterqueryObj.prototype.addUrlGetters = function(queue, args, startdata) {
    var that = this;
    var i;
    var urls;
    var url;
    if (!startdata) {
      urls = [];
      for(i = 0; i < args.all.length; i++) {
        var key = args.all[i][0];
        if(key === 'url') {
          var url = args.all[i][1];
          if (url.indexOf('//') == 0) url = window.location.protocol + url;
          url = this.extendDataUrl(url);
          urls.push(url);
        }
      }
      //console.debug("Original data URLs:", urls);
      if(urls.length === 0) {
         throw new Error('Missing url= in query parameter');
      }

      var state = {
        todo: urls,
        success: [],
        failure: [],
        rawdata: [],
      };

      this.enqueue(queue, 'get data', function(_, done) {
        that.getUrlData(state, that.wrap(done), function(s){that.gotError(s);});
      });
    } else {
      this.enqueue(queue, 'init data', function(_, done) {
        done([startdata]);
      });
    }

    this.enqueue(queue, 'parse', function(rawdata, done) {
      AfterqueryObj.log('rawdata:', rawdata);
      var outgrid = that.gridFromManyData(rawdata);
      AfterqueryObj.log('grid:', outgrid);
      done(outgrid);
    });
  };



AfterqueryObj.prototype.addKeepData = function(queue, args) {
  var box = {};
  this.enqueue(queue, 'keep', function(grid, done) {
    box.value = grid;
    done(grid);
    }
  );
  return box;
}

AfterqueryObj.prototype.exec = function(query, startdata, done) {
    var args = AfterqueryObj.parseArgs(query);
    var queue = [];
    this.addUrlGetters(queue, args, startdata);
    this.addTransforms(queue, args);
    this.runqueue(queue, startdata, done);
  };


AfterqueryObj.prototype.render = function(query, startdata, done, more_options) {
    var args = AfterqueryObj.parseArgs(query);
    var editlink = args.get('editlink');
    if (editlink == 0) {
      $(thid.elid('editmenu')).hide();
    }

    var queue = [];
    this.addUrlGetters(queue, args, startdata);
    this.addTransforms(queue, args);
    this.addRenderers(queue, args, more_options);
    this.finishQueue(queue, args, done);
  };


/* Runs get getter (url=X), then returns it. Useful for
   caching. */
AfterqueryObj.prototype.load = function(query, startdata, done) {
  var args = AfterqueryObj.parseArgs(query);
  var queue = [];
  this.addUrlGetters(queue, args, startdata);
  var results = this.addKeepData(queue, args);
  this.finishQueue(queue, args, done);
  return results;
}

/* Runs all of the data transforms, then returns it. Useful
   for creating a version to hand to some other system. */
AfterqueryObj.prototype.load_post_transform = function(query, startdata, done) {
  var args = AfterqueryObj.parseArgs(query);
  var queue = [];
  this.addUrlGetters(queue, args, startdata);
  this.addTransforms(queue, args);
  var results = this.addKeepData(queue, args);
  this.finishQueue(queue, args, done);
  return results;
}


// v8shell compatibility
try {
  var c = window.console;
  AfterqueryObj.console_debug = c.debug;
}
catch (ReferenceError) {
  AfterqueryObj.console.debug = print;
}
// Konqueror compatibility.
if(!AfterqueryObj.console_debug) {
  AfterqueryObj.console.debug = function(){};
}

// v8shell compatibility
try {
  AfterqueryObj.prototype.localStorage = window.localStorage;
} catch (ReferenceError) {
  AfterqueryObj.prototype.localStorage = {};
}


// Original Afterquery interface.
var afterquery = {};
afterquery.render = function(query, startdata, done) {
  var aq = new AfterqueryObj();
  aq.render(query, startdata, done);
}

////////////////////////////////////////////////////////////////////////////////

/*

heatgrid.js: Renders a heat grid.

 * Copyright 2013 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

var HeatGrid = function(el) {
  this.el = $(el);

  var frac = function(minval, maxval, fraction) {
    return minval + fraction * (maxval - minval);
  };

  var gradient = function(mincolor, zerocolor, maxcolor,
                          ofs) {
    if (ofs == 0) {
      return zerocolor;
    } else if (ofs < 0) {
      return [frac(zerocolor[0], mincolor[0], -ofs / 2),
              frac(zerocolor[1], mincolor[1], -ofs / 2),
              frac(zerocolor[2], mincolor[2], -ofs / 2)];
    } else if (ofs > 0) {
      return [frac(zerocolor[0], maxcolor[0], ofs / 2),
              frac(zerocolor[1], maxcolor[1], ofs / 2),
              frac(zerocolor[2], maxcolor[2], ofs / 2)];
    }
  };

  this.draw = function(grid) {
    console.debug('heatgrid.draw', grid);
    this.el.html('<div id="heatgrid" tabindex=0><canvas></canvas>' +
                 '<div id="heatgrid-popover"></div>' +
                 '<div id="heatgrid-highlight"></div></div>');
    var heatgrid = this.el.find('#heatgrid');
    heatgrid.css({
      position: 'relative',
      overflow: 'scroll',
      width: '100%',
      height: '100%'
    });
    var popover = this.el.find('#heatgrid-popover');
    popover.css({
      position: 'absolute',
      background: '#aaa',
      border: '1px dotted black',
      'white-space': 'pre'
    });
    var highlight = this.el.find('#heatgrid-highlight');
    highlight.css({
      position: 'absolute',
          //background: 'rgba(255,192,192,16)',
      width: '3px',
      height: '3px',
      margin: '0',
      padding: '0',
      border: '1px solid red'
    });
    var canvas = this.el.find('canvas');
    var xmult = parseInt(1000 / grid.headers.length);
    if (xmult < 1) xmult = 1;
    var xsize = grid.headers.length * xmult;
    var ysize = grid.data.length;
    var vysize = ysize < 400 ? 400 : ysize;
    canvas.attr({width: xsize, height: ysize});
    canvas.css({
      background: '#fff',
      width: '100%',
      height: vysize
    });
    console.debug('heatgrid canvas size is: x y =', xsize, ysize);
    var ctx = canvas[0].getContext('2d');

    if (!grid.data.length || !grid.data[0].length) {
      return;
    }

    var lastPos = { x: 0, y: 0 };
    var movefunc = function(x, y) {
      if (x >= grid.headers.length) x = grid.headers.length - 1;
      if (y >= grid.data.length) y = grid.data.length - 1;
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      lastPos.x = x;
      lastPos.y = y;
      var info = [];
      for (var i = 0; i < grid.headers.length; i++) {
        if (grid.types[i] != 'number') {
          info.push(grid.data[y][i]);
        } else {
          break;
        }
      }
      info.push(grid.headers[x]);
      info.push('value=' + grid.data[y][x]);

      var psize_x = canvas.width() / grid.headers.length;
      var psize_y = canvas.height() / grid.data.length;
      var cx = x * psize_x;
      var cy = y * psize_y;
      highlight.css({left: cx, top: cy});
      if (cx < canvas.width() / 2) {
        popover.css({
          left: cx + 30,
          right: 'auto'
        });
      } else {
        popover.css({
          left: 'auto',
          right: heatgrid[0].clientWidth - cx + 10
        });
      }
      if (cy < canvas.height() / 2) {
        popover.css({
          top: cy + 10,
          bottom: 'auto'
        });
      } else {
        popover.css({
          top: 'auto',
          bottom: heatgrid[0].clientHeight - cy + 10
        });
      }
      popover.text(info.join('\n'));
    };
    heatgrid.mousemove(function(ev) {
      var pos = canvas.position();
      var offX = ev.pageX - pos.left - 1;
      var offY = ev.pageY - pos.top - 1;
      var x = parseInt(offX / canvas.width() * grid.headers.length);
      var y = parseInt(offY / canvas.height() * grid.data.length);
      movefunc(x, y);
    });
    heatgrid.keydown(function(ev) {
      if (ev.which == 38) { // up
        movefunc(lastPos.x, lastPos.y - 1);
      } else if (ev.which == 40) { // down
        movefunc(lastPos.x, lastPos.y + 1);
      } else if (ev.which == 37) { // left
        movefunc(lastPos.x - 1, lastPos.y);
      } else if (ev.which == 39) { // right
        movefunc(lastPos.x + 1, lastPos.y);
      } else {
        return true; // propagate event forward
      }
      ev.stopPropagation();
      return false;
    });
    heatgrid.mouseleave(function() {
      popover.hide();
    });
    heatgrid.mouseenter(function() {
      heatgrid.focus(); // so that keyboard bindings work
      popover.show();
    });

    var total = 0, count = 0;
    for (var y = 0; y < grid.data.length; y++) {
      for (var x = 0; x < grid.data[y].length; x++) {
        if (grid.types[x] != 'number') continue;
        var cell = parseFloat(grid.data[y][x]);
        if (!isNaN(cell)) {
          total += cell;
          count++;
        }
      }
    }
    var avg = total / count;

    var tdiff = 0;
    for (var y = 0; y < grid.data.length; y++) {
      for (var x = 0; x < grid.data[y].length; x++) {
        if (grid.types[x] != 'number') continue;
        var cell = parseFloat(grid.data[y][x]);
        if (!isNaN(cell)) {
          tdiff += (cell - avg) * (cell - avg);
        }
      }
    }
    var stddev = Math.sqrt(tdiff / count);
    console.debug('total,count,mean,stddev = ', total, count, avg, stddev);

    var img = ctx.createImageData(xsize, ysize);
    for (var y = 0; y < grid.data.length; y++) {
      for (var x = 0; x < grid.data[y].length; x++) {
        if (grid.types[x] != 'number') continue;
        var cell = parseFloat(grid.data[y][x]);
        if (isNaN(cell)) continue;
        var ofs = stddev ? (cell - avg) / stddev : 3;
        var color = gradient([192, 192, 192],
                             [192, 192, 255],
                             [0, 0, 255], ofs);
        if (!color) continue;
        var pix = (y * xsize + x * xmult) * 4;
        for (var i = 0; i < xmult; i++) {
          img.data[pix + 0] = color[0];
          img.data[pix + 1] = color[1];
          img.data[pix + 2] = color[2];
          img.data[pix + 3] = 255;
          pix += 4;
        }
      }
    }
    ctx.putImageData(img, 0, 0);
  };
};

////////////////////////////////////////////////////////////////////////////////
