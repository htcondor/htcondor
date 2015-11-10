// TODO: Assumes a single, global #data-duration #data-source. Should be classes referenced uner #this.rootid

function HTCondorViewSimple(rootid) {
	var that = this;
	$(document).ready( function() { that.initialize(rootid); });
}

HTCondorViewSimple.next_graph_id = 0;

HTCondorViewSimple.prototype.new_graph_id = function() {
	HTCondorViewSimple.next_graph_id++;
	var new_id = "htcondorviewsimple" + HTCondorViewSimple.next_graph_id;
	return new_id;
}

HTCondorViewSimple.prototype.initialize = function(rootid) {
	var that = this;
	this.rootid = rootid;
	var container = $('#'+rootid);
	if(container.length == 0) {
		console.log('HTCondor View Simple is not able to intialize. There is no element with an ID of "'+id+'".');
		return;
	}
	var new_html = this.html_tabs();
	this.graph_id = this.new_graph_id();
	new_html += '<div id="'+this.graph_id+'"></div>';
	container.html(new_html);

	// Initialize tabs
	$('ul.tabs li').click(function(){
		$('ul.tabs li').removeClass('current');
		$(this).addClass('current');
	});

	$('ul.radio-tabs input').change(function() {
		that.active_filter = undefined;
		that.alt_title = undefined;
		that.change_view();
		});

	this.change_view();

};


HTCondorViewSimple.prototype.submitters_data_source = function() { "use strict"; return "submitters.json"; }
HTCondorViewSimple.prototype.submitters_now_data_source = function() { "use strict"; return "submitters.now.json"; }
HTCondorViewSimple.prototype.machines_data_source = function() { "use strict"; return "machines.json"; }
HTCondorViewSimple.prototype.machines_now_data_source = function() { "use strict"; return "machines.now.json"; }


/*
is_chart - true it's a chart (pie/stacked), false it's a table.
source - submitters or machines
duration - now, day, week, or month
*/
HTCondorViewSimple.prototype.graph_args = function(is_chart, source, duration) {
	"use strict";
	var title;
	switch(source) {
		case 'submitters':
			var machine_args = this.htcview_args(source, duration);
			if(is_chart) { return machine_args[0]; }
			return machine_args[1];
		case 'machines':
			var machine_args = this.htcview_args(source, duration);
			if(is_chart) { return machine_args[0]; }
			return machine_args[1];
	}
}

// Return arguments for HTCondorView's constructor. Is an array of
// 3 elements:
// 0. graph arguments
// 1. table arguments
// 2. select tuple (sub-array)
HTCondorViewSimple.prototype.htcview_args = function(source, duration) {
	"use strict";

	switch(source) {
	case 'submitters':
		switch(duration) {
		case "now":
			return [
				// Graph
				"url=" + this.submitters_now_data_source() + "&" +
				"title=Total Jobs&"+
				"order=JobStatus&"+
				"group=JobStatus;Count&"+
				"chart=pie",
				// Table
				"url=" + this.submitters_now_data_source() + "&" +
				"title=Total Jobs&"+
				"order=JobStatus&"+
				"pivot=Name;JobStatus;Count",
				// Select
				["Name"]
			];
			break;
		case "day":
		case "week":
		case "month":
			return [
				// Graph
				"url=" + this.submitters_data_source() + "&" +
				"title=Total Jobs&"+
				"order=Date&"+
				"pivot=Date;JobStatus;Count&"+
				"chart=stacked",
				// Table
				"url=" + this.submitters_data_source() + "&" +
				"title=Total Jobs&"+
				"order=Date&"+
				"pivot=Name;JobStatus;avg(Count)",
				// Select
				["Name"]
			];
		}
		break;
	case 'machines':
		switch(duration) {
		case "now":
			return [
				// Graph
				"title=Machine State&"+
					"url="+this.machines_now_data_source()+"&"+
					"order=State&"+
					"group=State;Cpus&"+
					"chart=pie",
				// Table
				"url="+this.machines_now_data_source()+"&"+
					"order=Arch,OpSys&"+
					"group=Arch,OpSys;State;Cpus",
				// Select
				["Arch", "OpSys"]
			];
		case "day":
		case "week":
		case "month":
			return [
				// Graph
				"title=Machine State&"+
					"url=" + this.machines_data_source() + "&" +
					"order=Date&" +
					"pivot=Date;State;Cpus&" +
					"chart=stacked&",
				// Table
				"url=" + this.machines_data_source() + "&" +
					"order=Date&" +
					"pivot=Date,Arch,OpSys;State;Cpus&" +
					"group=Arch,OpSys;avg(Unclaimed),avg(Claimed),max(Unclaimed),max(Claimed)",
				// Select
				["Arch", "OpSys"]
			];
		}
		break;
	}

}


HTCondorViewSimple.prototype.change_view = function() {
	// Purge everything.
	$("#"+this.graph_id).empty();
	this.htcondor_view = null;

	var duration = $('#data-duration input[type="radio"]:checked').val()
	var source = $('#data-source input[type="radio"]:checked').val()

	var graph_args = this.graph_args(true, source, duration);
	var table_args = this.graph_args(false, source, duration);
	console.log(graph_args);
	console.log(table_args);

	var select_tuple;
	switch(source) {
	case "machines": select_tuple = ["Arch", "OpSys"]; break;
	case "submitters": select_tuple = ["Name"]; break;
	}

	this.htcondor_view = new HTCondorView(this.graph_id, graph_args, table_args, select_tuple);
};


HTCondorViewSimple.prototype.html_tabs = function() {
	return "" +
	"<div style=\"text-align: center\">\n" +
	"<ul class=\"radio-tabs\" id=\"data-source\">\n" +
	"<li><input type=\"radio\" name=\"data-source\" id=\"data-source-user\" value=\"submitters\" checked> <label for=\"data-source-user\">Users</label>\n" +
	"<li><input type=\"radio\" name=\"data-source\" id=\"data-source-machine\" value=\"machines\"> <label for=\"data-source-machine\">Pool</label>\n" +
	"</ul>\n" +
	"<ul class=\"radio-tabs\" id=\"data-duration\">\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-now\" value=\"now\"> <label for=\"data-duration-now\">Now</label>\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-day\" value=\"day\" checked> <label for=\"data-duration-day\">Day</label>\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-week\" value=\"week\"> <label for=\"data-duration-week\">Week</label>\n" +
	"<li><input type=\"radio\" name=\"data-duration\" id=\"data-duration-month\" value=\"month\"> <label for=\"data-duration-month\">Month</label>\n" +
	"</ul>\n" +
	"</div>\n";
};
