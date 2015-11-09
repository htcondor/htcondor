function HTCondorView(id) {
	"use strict";
	this.urlTool = document.createElement('a');
	var mythis = this;

	var container = $('#'+id);
	if(container.length == 0) {
		console.log('HTCondor View is not able to intialize. There is no element with an ID of "'+id+'".');
		return false;
	}
	container.html(this.starting_html());

	window.onpopstate = function() {
		setTimeout(function(){
			mythis.load_arguments_to_form();
			mythis.load_and_render();
			},1);
	}

	$('.download-link').click(function(ev) { mythis.download_csv(mythis.data.value); ev.preventDefault();});

	// Initialize tabs
	$('ul.tabs li').click(function(){
		var tab_id = $(this).attr('data-tab');

		$('ul.tabs li').removeClass('current');
		$('.tab-content').removeClass('current');

		$(this).addClass('current');
		$("#"+tab_id).addClass('current');
	});

	$('ul.radio-tabs input').change(function() {
		mythis.active_filter = undefined;
		mythis.alt_title = undefined;
		mythis.change_view();
		});

	this.load_arguments_to_form();
	this.change_view()
	this.load_and_render();
}

HTCondorView.next_graph_id = 0;

HTCondorView.prototype.new_graph_id = function() {
	HTCondorView.next_graph_id++;
	var new_id = "htcondorview" + HTCondorView.next_graph_id;
	return new_id;
}

HTCondorView.prototype.toggle_edit = function(btn, controls) {
	"use strict";
	$(controls).toggle();
	if($(controls).is(":visible")) {
		$(btn).text("❌")
	} else {
		$(btn).text("edit")
	}
}

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
}

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

HTCondorView.prototype.load_and_render = function() {
	"use strict";

	this.save_arguments_to_url();

	var mythis = this;
	var callback_render_table = function() {
		$('#'+mythis.table_id+' .vizchart').empty();
		setTimeout(function() {
			var options = {
				select_handler: function(e,t,d){ mythis.table_select_handler(e,t,d); },
				num_pattern: '#,##0.0',
				disable_height: true
			};

			afterquery.render(mythis.current_tableargs, mythis.data.value, null, mythis.table_id, options);
		}, 0);
	 };

	var callback_render_graph = function(){
		$('#'+mythis.graph_id+' .vizchart').empty();
		setTimeout(function() {
			afterquery.render(mythis.current_graphargs, mythis.data.value, callback_render_table, mythis.graph_id);
			},0)
		};
	var args = this.current_graphargs;
	var newurl = afterquery.parseArgs(args).get('url');
	if(newurl == this.data_url) {
		callback_render_graph();
	} else {
		this.data_url = newurl;
		this.data = afterquery.load(args, null, function(){
			callback_render_graph();
			}, this.graph_id);
	}
}

HTCondorView.prototype.table_select_handler = function(evnt,table,data) {
	"use strict";
	var selection = table.getSelection();
	if(selection) {
		var source = $('#data-source input[type="radio"]:checked').val()
		var duration = $('#data-duration input[type="radio"]:checked').val()
		if(source == "machines") {
			var row = selection[0].row;
			var arch = data.getValue(row, 0);
			var opsys = data.getValue(row, 1);
			this.active_filter = {Arch: arch, OpSys: opsys};
			this.alt_title = "Machine State for "+arch+"/"+opsys;
			this.current_graphargs = this.graph_args(true, source, duration, this.active_filter, this.alt_title);
			this.load_and_render();
		} else if(source =="submitters") {
			var row = selection[0].row;
			var user = data.getValue(row, 0);
			this.active_filter = {Name:user};
			this.alt_title = "Jobs for "+user;
			this.current_graphargs = this.graph_args(true, source, duration, this.active_filter, this.alt_title);
			this.load_and_render();
		}
	}
}

HTCondorView.prototype.change_view = function() {
	"use strict";
	var duration = $('#data-duration input[type="radio"]:checked').val()
	var source = $('#data-source input[type="radio"]:checked').val()
	if(source == "machines" || source == "submitters") {
		this.current_graphargs = this.graph_args(true, source, duration, this.active_filter, this.alt_title);
		this.current_tableargs = this.graph_args(false, source, duration, this.active_filter, this.alt_title);
		this.load_and_render();
	} else if(source=="custom") {
		$("#"+this.graph_id+" .vizchart").html("<h2>Not yet implemented</h2>");
		$("#"+this.table_id+" .vizchart").html("<h2>Not yet implemented</h2>");
	}
}

HTCondorView.prototype.submitters_data_source = function() { "use strict"; return "submitters.json"; }
HTCondorView.prototype.submitters_now_data_source = function() { "use strict"; return "submitters.now.json"; }
HTCondorView.prototype.machines_data_source = function() { "use strict"; return "machines.json"; }
HTCondorView.prototype.machines_now_data_source = function() { "use strict"; return "machines.now.json"; }

/*
is_chart - true it's a chart (pie/stacked), false it's a table.
source - submitters or machines
duration - now, day, week, or month
filters - optional. Hash of fields to filter on
   mapped to values to filter by.
title - optional title for chart.
*/
HTCondorView.prototype.graph_args = function(is_chart, source, duration, filters, title) {
	"use strict";
	var filter = '';
	if(filters !== undefined) {
		var key;
		for(key in filters) {
			filter += "filter=" + key + "=" + filters[key] + "&";
		}
	}
	switch(source) {
		case 'submitters':
		{
			if(title === undefined) { title = 'Total Jobs'; }
			var charttype = '';
			if(is_chart) { charttype = 'chart=stacked'; }
			if(duration == 'now') {
				var grouping = 'pivot=Name;JobStatus;Count';
				if(is_chart) {
					charttype = 'chart=pie';
					grouping = "group=JobStatus;Count";
				} else {
					filter = '';
				}
				return "title=" + title + "&" +
					"url=" + this.submitters_now_data_source() + "&" +
					filter +
					"order=JobStatus&" +
					grouping + "&" +
					charttype + "&";

			} else {
				var pivot = "Name;JobStatus;avg(Count)";
				if(is_chart) {
					pivot = "Date;JobStatus;Count";
				} else {
					filter = '';
				}
				return "url=" + this.submitters_data_source() + "&" +
					"title=" + title + "&" +
					filter +
					"order=Date&" +
					"pivot=" + pivot + "&" +
					charttype + "&";
			}
			break;
		}
		case 'machines':
		{
			if(title === undefined) { title = 'Machine State'; }
			if(duration == 'now') {
				if(is_chart) {
					return "title="+title+"&"+
						"url=machines.now.json&"+
						"order=State&"+
						"group=State;Cpus&"+
						"chart=pie";
				} else {
					return "title="+title+"&"+
						"url=machines.now.json&"+
						"order=Arch,OpSys&"+
						"group=Arch,OpSys;State;Cpus";
				}
			
			} else {
				if(is_chart) {
					return "title=" + title + "&" +
						"url=" + this.machines_data_source() + "&" +
						filter +
						"order=Date&" +
						"pivot=Date;State;Cpus&" +
						"chart=stacked&";
				} else {
					return "url=" + this.machines_data_source() + "&" +
						"order=Date&" +
						"pivot=Date,Arch,OpSys;State;Cpus&" +
						"group=Arch,OpSys;avg(Unclaimed),avg(Claimed),max(Unclaimed),max(Claimed)";
				}
			}
		}
	}
}

HTCondorView.prototype.html_for_graph = function(id, myclass) {
	return "" +
		"<div id='"+id+"' class='"+myclass+"'>\n" +
		"<div class='vizstatus'>\n" +
		"  <div class='statustext'></div>\n" +
		"  <div class='statussub'></div>\n" +
		"</div>\n" +
		"<div class='vizraw'></div>\n" +
		"<div class='vizchart'></div>\n" +
		"</div>\n";
}

HTCondorView.prototype.starting_html = function() {
	this.graph_id = this.new_graph_id();
	this.table_id = this.new_graph_id();
	"use strict";
	return "" +
	"<div style=\"text-align: center\">\n" +
	"<ul class=\"radio-tabs\" id=\"data-source\">\n" +
	"<li><input type=\"radio\" name=\"data-source\" id=\"data-source-user\" value=\"submitters\"> <label for=\"data-source-user\">Users</label>\n" +
	"<li><input type=\"radio\" name=\"data-source\" id=\"data-source-machine\" value=\"machines\"> <label for=\"data-source-machine\">Pool</label>\n" +
	"<li><input type=\"radio\" name=\"data-source\" id=\"data-source-custom\" value=\"custom\"> <label for=\"data-source-custom\">Custom</label>\n" +
	"</ul>\n" +
	"<ul class=\"radio-tabs\" id=\"data-duration\">\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-now\" value=\"now\"> <label for=\"data-duration-now\">Now</label>\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-day\" value=\"day\"> <label for=\"data-duration-day\">Day</label>\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-week\" value=\"week\"> <label for=\"data-duration-week\">Week</label>\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-month\" value=\"month\"> <label for=\"data-duration-month\">Month</label>\n" +
	"</ul>\n" +
	"</div>\n" +
	"\n" +
	"<div id=\"tab-user\" class=\"tab-content current\">\n" +
	"\n" +
	"<div class='editmenu'>" +
	"<button onclick=\"alert('Not yet implemented')\" class=\"editlink\">full screen</button>\n" +
	"</div>\n" +
	this.html_for_graph(this.graph_id, "graph")+ "\n" +
	"\n" +
	"<div class=\"download-link\"> <a href=\"#\">Download this table</a> </div>\n" +
	this.html_for_graph(this.table_id, "table")+ "\n" +
	"\n" +
	"</div> <!-- #tab-user .tab-content -->\n" +
	"\n" +
	"<div id=\"tab-machine\" class=\"tab-content\">\n" +
	"</div>\n" +
	"\n" +
	"<div id=\"tab-custom\" class=\"tab-content\">\n" +
	"TODO Custom\n" +
	"</div>\n" +
	"\n" +
	"<div id='vizlog'></div>\n" +
	"";
}


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

	for(var col = 0; col < columns; col++) {
		if(col > 0) { ret += ","; }
		ret += csv_escape(dt.headers[col]);
	}
	ret += "\n";

	for(var row = 0; row < rows; row++) {
		for(var col = 0; col < columns; col++) {
			if(col > 0) { ret += ","; }
			ret += csv_escape(dt.data[row][col]);
		}
		ret += "\n";
	}

	return ret;
}

HTCondorView.prototype.download_data = function(filename, type, data) {
	"use strict";
	var link = document.createElement('a');
	link.download = filename;
	link.href = "data:"+type+";charset=utf-8,"+encodeURIComponent(data);
	document.body.appendChild(link);
	link.click();
	document.body.removeChild(link);
}

HTCondorView.prototype.download_csv = function(data) {
	"use strict";
	var mythis = this;
	var handle_csv = function() {
		var csv = mythis.afterquerydata_to_csv(mythis.csv_source_data.value);
		mythis.csv_source_data = undefined;
		mythis.download_data("HTCondor-View-Data.csv", "text/csv", csv);
	}
	this.csv_source_data = afterquery.load_post_transform(this.current_tableargs, data, handle_csv, null);
}



