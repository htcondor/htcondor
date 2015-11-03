function HTCondorView(id) {
	this.urlTool = document.createElement('a');
	var mythis = this;

	$('#editlinkgraph1').click(function() { mythis.toggle_edit('#editlinkgraph1', '#graph1editor'); });
	$('#editlinktable1').click(function() { mythis.toggle_edit('#editlinktable1', '#table1editor'); });

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

	$('#reloadgraph1').click(function() {
		mythis.load_and_render();
		mythis.save_arguments_to_url();
		});
	$('#rerendergraph1').click(function() { mythis.render_new_graph('#graph1text', 'graph1'); });

	$('#reloadtable1').click(function() {
		mythis.load_and_render();
		mythis.save_arguments_to_url();
		});
	$('#rerendertable1').click(function() { mythis.render_new_graph('#table1text', 'table1'); });

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

HTCondorView.prototype.toggle_edit = function(btn, controls) {
	$(controls).toggle();
	if($(controls).is(":visible")) {
		$(btn).text("❌")
	} else {
		$(btn).text("edit")
	}
}

HTCondorView.prototype.load_arguments_to_form = function() {
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
	var url = window.location.href;

	var source = $('#data-source input[type="radio"]:checked').val()
	var duration = $('#data-duration input[type="radio"]:checked').val()

	url = this.replace_search_arg(url, 'source', source);
	url = this.replace_search_arg(url, 'duration', duration);

	var filter;
	if(this.active_filter !== undefined) {
		filter = '';
		for(key in this.active_filter) {
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

HTCondorView.prototype.read_arguments = function(source) {
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

HTCondorView.prototype.load_and_render = function() {
	var mythis = this;
	var callback_render_table = function() {
		setTimeout(function() {
			var options = {
				select_handler: function(e,t,d){ mythis.table_select_handler(e,t,d); },
				num_pattern: '#,##0.0',
				disable_height: true
			};

			afterquery.render(mythis.read_arguments('#table1text'), mythis.data.value, null, 'table1', options);
		}, 0);
	 };

	var callback_render_graph = function(){
		setTimeout(function() {
			afterquery.render(mythis.read_arguments('#graph1text'), mythis.data.value, callback_render_table, 'graph1');
			},0)
		};
	var args = this.read_arguments('#graph1text');
	var newurl = afterquery.parseArgs(args).get('url');
	if(newurl == this.data_url) {
		callback_render_graph();
	} else {
		this.data_url = newurl;
		this.data = afterquery.load(args, null, function(){callback_render_graph();}, 'graph1');
	}
}

HTCondorView.prototype.table_select_handler = function(evnt,table,data) {
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
			var new_graph_args = this.graph_args(true, source, duration, this.active_filter, this.alt_title);
			this.render_new_graph('#graph1text', 'graph1', new_graph_args);
		} else if(source =="submitters") {
			var row = selection[0].row;
			var user = data.getValue(row, 0);
			this.active_filter = {Name:user};
			this.alt_title = "Jobs for "+user;
			var new_graph_args = this.graph_args(true, source, duration, this.active_filter, this.alt_title);
			this.render_new_graph('#graph1text', 'graph1', new_graph_args);
		}
	}
}

HTCondorView.prototype.render_new_graph = function(editid, graphid, args) {
	if(args && args.length) {
		$(editid).val(args);
	}
	this.save_arguments_to_url();
	/*afterquery.render(this.read_arguments(editid), data.value,null,graphid);*/
	var to_prune = "#" + graphid + ' .vizchart';
	$(to_prune).empty();
	/* TODO: use cached data if posible */
	this.load_and_render();
}

HTCondorView.prototype.change_view = function() {
	var duration = $('#data-duration input[type="radio"]:checked').val()
	var source = $('#data-source input[type="radio"]:checked').val()
	if(source == "machines" || source == "submitters") {
		this.render_new_graph('#graph1text', 'graph1', this.graph_args(true, source, duration, this.active_filter, this.alt_title));
		this.render_new_graph('#table1text', 'table1', this.graph_args(false, source, duration, this.active_filter, this.alt_title));
	} else if(source=="custom") {
		$("#graph1 .vizchart").html("<h2>Not yet implemented</h2>");
		$("#table1 .vizchart").html("<h2>Not yet implemented</h2>");
	}
}

HTCondorView.prototype.submitters_data_source = function() { return "submitters.json"; }
HTCondorView.prototype.submitters_now_data_source = function() { return "submitters.now.json"; }
HTCondorView.prototype.machines_data_source = function() { return "machines.json"; }
HTCondorView.prototype.machines_now_data_source = function() { return "machines.now.json"; }

/*
is_chart - true it's a chart (pie/stacked), false it's a table.
source - submitters or machines
duration - now, day, week, or month
filters - optional. Hash of fields to filter on
   mapped to values to filter by.
title - optional title for chart.
*/
HTCondorView.prototype.graph_args = function(is_chart, source, duration, filters, title) {
	var filter = '';
	if(filters !== undefined) {
		var key;
		for(key in filters) {
			filter += "filter=" + key + "=" + filters[key] + "\n";
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
				return "title=" + title + "\n" +
					"url=" + this.submitters_now_data_source() + "\n" +
					filter +
					"order=JobStatus\n" +
					grouping + "\n" +
					charttype + "\n";

			} else {
				var pivot = "Name;JobStatus;avg(Count)";
				if(is_chart) {
					pivot = "Date;JobStatus;Count";
				} else {
					filter = '';
				}
				return "url=" + this.submitters_data_source() + "\n" +
					"title=" + title + "\n" +
					filter +
					"order=Date\n" +
					"pivot=" + pivot + "\n" +
					charttype + "\n";
			}
			break;
		}
		case 'machines':
		{
			if(title === undefined) { title = 'Machine State'; }
			if(duration == 'now') {
				if(is_chart) {
					return "title="+title+"\n"+
						"url=machines.now.json\n"+
						"order=State\n"+
						"group=State;Cpus\n"+
						"chart=pie";
				} else {
					return "title="+title+"\n"+
						"url=machines.now.json\n"+
						"order=Arch,OpSys\n"+
						"group=Arch,OpSys;State;Cpus";
				}
			
			} else {
				if(is_chart) {
					return "title=" + title + "\n" +
						"url=" + this.machines_data_source() + "\n" +
						filter +
						"order=Date\n" +
						"pivot=Date;State;Cpus\n" +
						"chart=stacked\n";
				} else {
					return "url=" + this.machines_data_source() + "\n" +
						"order=Date\n" +
						"pivot=Date,Arch,OpSys;State;Cpus\n" +
						"group=Arch,OpSys;avg(Unclaimed),avg(Claimed),max(Unclaimed),max(Claimed)";
				}
			}
		}
	}
}

HTCondorView.prototype.starting_html = function() {
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
	"<div class='editmenu'><button class=\"editlink\" id='editlinkgraph1'>edit</button>\n" +
	"<div id=\"graph1editor\" style=\"display:none;\">\n" +
	"<textarea id='graph1text' cols=\"40\" rows=\"10\" wrap='off'>\n" +
	"</textarea>\n" +
	"<div>\n" +
	"<button id=\"rerendergraph1\">Update Graph</button>\n" +
	"<button id=\"reloadgraph1\">Reload Data</button>\n" +
	"</div>\n" +
	"</div>\n" +
	"<br><button onclick=\"alert('Not yet implemented')\" class=\"editlink\">full screen</button>\n" +
	"</div>\n" +
	"\n" +
	"<div id='graph1'>\n" +
	"<div class='vizstatus'>\n" +
	"  <div class='statustext'></div>\n" +
	"  <div class='statussub'></div>\n" +
	"</div>\n" +
	"<div class='vizraw'></div>\n" +
	"<div class='vizchart'></div>\n" +
	"</div>\n" +
	"\n" +
	"<div class=\"download-link\"> <a href=\"#\">Download this table</a> </div>\n" +
	"<div class='editmenu'><button class=\"editlink\" id='editlinktable1'>edit</button>\n" +
	"<div id=\"table1editor\" style=\"display:none;\">\n" +
	"<textarea id='table1text' cols=\"40\" rows=\"10\" wrap='off'>\n" +
	"</textarea>\n" +
	"<div>\n" +
	"<button id=\"rerendertable1\">Update Table</button>\n" +
	"<button id=\"reloadtable1\">Reload Data</button>\n" +
	"</div>\n" +
	"</div>\n" +
	"</div>\n" +
	"\n" +
	"<div id='table1'>\n" +
	"<div class='vizstatus'>\n" +
	"  <div class='statustext'></div>\n" +
	"  <div class='statussub'></div>\n" +
	"</div>\n" +
	"<div class='vizraw'></div>\n" +
	"<div class='vizchart'></div>\n" +
	"</div>\n" +
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
	ret = '';
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
	var link = document.createElement('a');
	link.download = filename;
	link.href = "data:"+type+";charset=utf-8,"+encodeURIComponent(data);
	document.body.appendChild(link);
	link.click();
	document.body.removeChild(link);
}

HTCondorView.prototype.download_csv = function(data) {
	var mythis = this;
	var handle_csv = function() {
		var csv = mythis.afterquerydata_to_csv(mythis.csv_source_data.value);
		mythis.csv_source_data = undefined;
		mythis.download_data("HTCondor-View-Data.csv", "text/csv", csv);
	}
	var args = this.read_arguments('#table1text');
	this.csv_source_data = afterquery.load_post_transform(args, data, handle_csv, null);
}



